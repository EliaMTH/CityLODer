#!/usr/bin/env pwsh

param(
    [switch]$Help, 

    [string]$LAS_PATH,
    [string]$INPUT2D_PATH,
    [string]$OUTPUT_FILE_PATH = "output",
    [string]$OUTPUT_FILE_NAME = "mycity",
    [string]$GRAPH_PATH = ".",
    [string]$CLASS_BUILDING = "6",
    [string]$CLASS_GROUND = "2",
    [string]$TEMP_FOLD = "."
)

function Show-Usage {
@"
Usage:
  .\cityloder_app.ps1 -LAS_PATH <file> -INPUT2D_PATH <file> [-OUTPUT_FILE_PATH <path>] [-OUTPUT_FILE_NAME <name>] [-GRAPH_PATH <path>] [-CLASS_BUILDING <int>] [-CLASS_GROUND <int>] [-TEMP_FOLD <path>]

Warning 1: LAS_PATH, INPUT2D_PATH, OUTPUT_FILE_PATH, and GRAPH_PATH are assumed to be relative to the current folder.
Warning 2: The docker image is assumed to be called "cityloder". Change the line accordingly if you used another name.

Arguments:
  LAS_PATH          Required. LAS input file
  INPUT2D_PATH      Required. Footprints input file
  OUTPUT_FILE_PATH  Optional. Default: output
  OUTPUT_FILE_NAME  Optional. Default: mycity
  GRAPH_PATH        Optional. Default: .
  CLASS_BUILDING    Optional. Default: 6
  CLASS_GROUND      Optional. Default: 2
  TEMP_FOLD         Optional. Default: (inside container)
"@
}

if ($Help) {
    Show-Usage
    exit 0
}
if ([string]::IsNullOrWhiteSpace($LAS_PATH) -or [string]::IsNullOrWhiteSpace($INPUT2D_PATH)) {
    Write-Error "LAS_PATH and INPUT2D_PATH are required."
    Show-Usage
    exit 1
}


$currentDir = (Get-Location).Path

$lasArg     = "/data/$LAS_PATH"
$input2dArg = "/data/$INPUT2D_PATH"
$outputArg  = "/data/$OUTPUT_FILE_PATH"
$graphArg   = if ($GRAPH_PATH -eq ".") { "." } else { "/data/$GRAPH_PATH" }
$tmpfoldArg = if ($TEMP_FOLD -eq ".") { "." } else { "/data/$TEMP_FOLD" }

docker run --rm `
  -v "${currentDir}:/data" `
  cityloder `
  "$lasArg" `
  "$input2dArg" `
  "$outputArg" `
  "$OUTPUT_FILE_NAME" `
  "$graphArg" `
  "$CLASS_BUILDING" `
  "$CLASS_GROUND" `
  "$tmpfoldArg"