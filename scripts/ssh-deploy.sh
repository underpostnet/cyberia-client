
#!/bin/bash
# Deploy Cyberia Client via GitHub Actions workflow dispatch
set -e

cd /home/dd/engine/cyberia-client
underpost push . underpostnet/cyberia-client
gh workflow run cyberia-client.cd.yml -R underpostnet/cyberia-client -f job=deploy
