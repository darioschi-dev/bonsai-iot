Import("env")
from SCons.Script import AlwaysBuild, Default

# Comando per eseguire automaticamente l'upload di SPIFFS
def after_upload(source, target, env):
    env.Execute("pio run --target uploadfs")

# Associa lo script da eseguire dopo l'upload del codice
env.AddPostAction("upload", after_upload)
