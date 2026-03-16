#!/bin/bash -x
# Example call Github Actions API to call example action

OWNER=underpostnet
REPO=cyberia-client
WORKFLOW_ID=example-api.yml
BRANCH=master

USER_TOKEN=${1:-default}

curl -L \
  -X POST \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer ${USER_TOKEN}" \
  -H "X-GitHub-Api-Version: 2026-03-10" \
  https://api.github.com/repos/${OWNER}/${REPO}/actions/workflows/${WORKFLOW_ID}/dispatches \
  -d '{"ref":"'${BRANCH}'","inputs":{"input1":"hello","input2":"world"}}'