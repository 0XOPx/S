@echo off
setlocal

set "msg=git-do.cmd message blank"
if /i "%~1"=="--message" (
    if not "%~2"=="" set "msg=%~2"
)

git add -A
git reset dist
git reset *.exe
git reset *.zip
git commit -m "%msg%"
