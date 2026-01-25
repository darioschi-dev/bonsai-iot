#!/bin/bash

set -e

echo "📦 Backup della cartella .git esistente..."
mv .git .git.bak

echo "🚀 Inizializzazione nuovo repository Git..."
git init
git remote add origin https://github.com/darioschi-dev/bonsai-firmware.git

echo "🧼 Aggiunta dei file puliti al commit..."
git add .
git commit -m '🚀 Reinizializzato repo pulito per supporto OTA'

echo "📤 Push forzato verso origin/master..."
git push -f origin master

echo "✅ Operazione completata. Il repository è stato ripulito e sincronizzato."
