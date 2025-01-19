from SCons.Script import DefaultEnvironment
import subprocess

# Inizializza l'ambiente correttamente
env = DefaultEnvironment()

def upload_fs(source, target, env):
    result = subprocess.run(
        ["pio", "run", "-t", "uploadfs"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    if result.returncode != 0:
        print(result.stderr)
        raise Exception("Errore durante l'upload dei file SPIFFS")

# Aggiungi l'azione post correttamente
env.AddPostAction("upload", upload_fs)
