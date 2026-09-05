from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


@dataclass(frozen=True)
class Change:
    status: str
    path: str


@dataclass(frozen=True)
class PreCommitResult:
    ok: bool
    checked_assets: int
    errors: list[str]


def _normalize(path: str) -> str:
    normalized = str(PurePosixPath(path.replace("\\", "/")))
    while normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized


def parse_manifest(text: str) -> list[Change]:
    changes: list[Change] = []
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        status = fields[0].upper()
        if status.startswith("R") and len(fields) == 3:
            changes.append(Change("D", _normalize(fields[1])))
            changes.append(Change("A", _normalize(fields[2])))
        elif status[:1] in {"A", "M", "D"} and len(fields) == 2:
            changes.append(Change(status[:1], _normalize(fields[1])))
        else:
            raise ValueError(
                f"line {line_number}: expected STATUS<TAB>PATH or R<TAB>OLD<TAB>NEW"
            )
    return changes


def git_staged_changes(project_root: Path) -> list[Change]:
    process = subprocess.run(
        ["git", "diff", "--cached", "--name-status", "-z", "--diff-filter=AMDR"],
        cwd=project_root,
        check=True,
        capture_output=True,
    )
    fields = process.stdout.decode("utf-8", errors="strict").split("\0")
    changes: list[Change] = []
    index = 0
    while index < len(fields) and fields[index]:
        status = fields[index].upper()
        index += 1
        if status.startswith("R"):
            if index + 1 >= len(fields):
                raise ValueError("incomplete Git rename record")
            changes.append(Change("D", _normalize(fields[index])))
            changes.append(Change("A", _normalize(fields[index + 1])))
            index += 2
        else:
            if index >= len(fields):
                raise ValueError("incomplete Git change record")
            changes.append(Change(status[:1], _normalize(fields[index])))
            index += 1
    return changes


def _asset_mapping(path: str) -> tuple[str, str] | None:
    pure = PurePosixPath(path)
    parts = pure.parts
    if not parts or parts[0].lower() != "content":
        return None
    extension = pure.suffix.lower()
    if extension not in {".uasset", ".umap"}:
        return None
    relative = PurePosixPath(*parts[1:]).with_suffix("")
    package_name = "/Game/" + relative.as_posix()
    output_group = "maps" if extension == ".umap" else "content"
    semantic_path = f".uesem/{output_group}/Game/{relative.as_posix()}.uesem.json"
    return package_name, semantic_path


def _semantic_mapping(path: str) -> tuple[str, str] | None:
    pure = PurePosixPath(path)
    parts = pure.parts
    if len(parts) < 4 or parts[0:2] not in {
        (".uesem", "content"),
        (".uesem", "maps"),
    }:
        return None
    if parts[2].lower() != "game" or not pure.name.endswith(".uesem.json"):
        return None
    relative_text = PurePosixPath(*parts[3:]).as_posix()
    relative = PurePosixPath(relative_text[: -len(".uesem.json")])
    extension = ".umap" if parts[1] == "maps" else ".uasset"
    asset_path = f"Content/{relative.as_posix()}{extension}"
    package_name = "/Game/" + relative.as_posix()
    return package_name, asset_path


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return f"sha256:{digest.hexdigest()}"


def validate_changes(
    project_root: str | Path,
    changes: list[Change],
    *,
    require_index: bool = False,
) -> PreCommitResult:
    root = Path(project_root).resolve()
    normalized_changes = {_normalize(change.path): change.status for change in changes}
    errors: list[str] = []
    checked_asset_paths: set[str] = set()

    for change in changes:
        mapping = _asset_mapping(change.path)
        if mapping is None:
            continue
        checked_asset_paths.add(_normalize(change.path))
        package_name, semantic_path = mapping
        semantic_status = normalized_changes.get(semantic_path)
        if semantic_status is None:
            errors.append(
                f"{change.path}: paired semantic file is not in the changelist: {semantic_path}"
            )
            continue
        if change.status == "D":
            if semantic_status != "D":
                errors.append(
                    f"{change.path}: deleted asset requires deleted/renamed semantic file: "
                    f"{semantic_path}"
                )
            continue

        source_file = root / Path(change.path)
        semantic_file = root / Path(semantic_path)
        if not source_file.is_file():
            errors.append(f"{change.path}: changed asset does not exist")
            continue
        if not semantic_file.is_file():
            errors.append(f"{change.path}: semantic file does not exist: {semantic_path}")
            continue
        try:
            semantic = json.loads(semantic_file.read_text(encoding="utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            errors.append(f"{semantic_path}: invalid UTF-8 JSON: {exc}")
            continue
        asset = semantic.get("asset") if isinstance(semantic, dict) else None
        if not isinstance(asset, dict):
            errors.append(f"{semantic_path}: missing asset object")
            continue
        if asset.get("packageName") != package_name:
            errors.append(
                f"{semantic_path}: packageName mismatch; expected {package_name}, "
                f"got {asset.get('packageName')}"
            )
        source_hash = _sha256(source_file)
        if asset.get("sourceHash") != source_hash:
            errors.append(
                f"{semantic_path}: stale sourceHash; expected {source_hash}, "
                f"got {asset.get('sourceHash')}"
            )

    for change in changes:
        mapping = _semantic_mapping(change.path)
        if mapping is None:
            continue
        package_name, asset_path = mapping
        asset_status = normalized_changes.get(asset_path)
        if asset_status is None:
            errors.append(
                f"{change.path}: paired Unreal asset is not in the changelist: {asset_path}"
            )
            continue
        if change.status == "D" and asset_status != "D":
            errors.append(
                f"{change.path}: deleted/renamed semantic file requires deleted/renamed asset: "
                f"{asset_path}"
            )
        elif change.status != "D" and asset_status == "D":
            errors.append(
                f"{change.path}: added/modified semantic file cannot pair with deleted asset: "
                f"{asset_path}"
            )
        checked_asset_paths.add(asset_path)

    checked_assets = len(checked_asset_paths)
    if require_index and checked_assets:
        index_path = ".uesem/index/project.uesem.index.json"
        if index_path not in normalized_changes:
            errors.append(f"asset changes require the project index in the changelist: {index_path}")

    errors.sort()
    return PreCommitResult(not errors, checked_assets, errors)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify that changed Unreal assets and UE Ring semantic sidecars are synchronized."
    )
    parser.add_argument("--project-root", default=".", help="Unreal project root.")
    parser.add_argument(
        "--manifest",
        help="UTF-8 change manifest: STATUS<TAB>PATH, or R<TAB>OLD<TAB>NEW.",
    )
    parser.add_argument(
        "--require-index",
        action="store_true",
        help="Also require .uesem/index/project.uesem.index.json in the changelist.",
    )
    args = parser.parse_args()

    project_root = Path(args.project_root).resolve()
    try:
        changes = (
            parse_manifest(Path(args.manifest).read_text(encoding="utf-8"))
            if args.manifest
            else git_staged_changes(project_root)
        )
        result = validate_changes(project_root, changes, require_index=args.require_index)
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: could not read changelist: {exc}")
        return 2

    if result.ok:
        print(f"OK: checked {result.checked_assets} changed Unreal asset(s)")
        return 0
    for error in result.errors:
        print(f"ERROR: {error}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
