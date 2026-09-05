#include the Flask, request, and jsonify modules from the flask libraray
from flask import Flask, request, jsonify
import subprocess
from consts import SESSION_NAME_KEY, SESSION_SEARCH_ID_KEY, PORT_KEY
import re

app=Flask(__name__)

# TODO: Remove when using docker in the future
nextAvailablePort = 7777

def CreateSeverLocalTest(sessionName, sessionSearchId):
    global nextAvailablePort
    subprocess.Popen([
        "F:/UnrealSrc/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe",
        "D:/CompleteGameDevSeries06_Crunch/Crunch/Crunch.uproject" ,
        "-server",
        "-log",
        '-epicapp="ServerClient"',
        f'-SESSION_NAME="{sessionName}"',
        f'-SESSION_SEARCH_ID="{sessionSearchId}"',
        f'-PORT={nextAvailablePort}'
    ])

    usedPort = nextAvailablePort
    nextAvailablePort += 1
    return usedPort


@app.route('/Sessions', methods=['POST'])
def CreateSever():
    print(dict(request.headers))

    sessionName = request.get_json().get(SESSION_NAME_KEY)
    sessionSearchId = request.get_json().get(SESSION_SEARCH_ID_KEY)

    port = CreateSeverLocalTest(sessionName, sessionSearchId)
    return jsonify({"status": "success", PORT_KEY: port}), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=80)
