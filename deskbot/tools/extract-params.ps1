# Extract customizable parameters from an OpenSCAD file (Windows)
# Usage: .\extract-params.ps1 input.scad [-Json]
#
# Parses parameter declarations with special comments:
#   param = value;   // [min:max] Description
#   param = value;   // [min:step:max] Description
#   param = value;   // [opt1, opt2] Description
#   param = value;   // Description only

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$InputFile,

    [switch]$Json
)

if (-not (Test-Path $InputFile)) {
    Write-Error "Error: File not found: $InputFile"
    exit 1
}

# Use Python for parsing (same logic as original bash script)
$pythonScript = @'
import sys
import re
import json

filename = sys.argv[1]
json_output = len(sys.argv) > 2 and sys.argv[2] == "--json"

in_block = 0
params = []

with open(filename, "r", encoding="utf-8") as f:
    for line in f:
        in_block += line.count("{") - line.count("}")
        if in_block > 0:
            continue

        match = re.match(r"^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;]+);\s*(?://\s*(.*))?", line)
        if not match:
            continue

        var_name  = match.group(1)
        value     = match.group(2).strip()
        comment   = match.group(3) or ""

        if value in ("true", "false"):
            var_type = "boolean"
        elif re.match(r"^-?\d+$", value):
            var_type = "integer"
        elif re.match(r"^-?\d*\.?\d+$", value):
            var_type = "number"
        elif value.startswith('"') and value.endswith('"'):
            var_type = "string"
            value = value[1:-1]
        elif value.startswith("["):
            var_type = "array"
        else:
            var_type = "expression"

        range_val = ""
        options_val = ""
        description = comment

        range_match = re.match(r"\[([^\]]+)\]\s*(.*)", comment)
        if range_match:
            bracket_content = range_match.group(1)
            description = range_match.group(2)
            if ":" in bracket_content and "," not in bracket_content:
                range_val = bracket_content
            else:
                options_val = bracket_content

        params.append({
            "name": var_name,
            "value": value,
            "type": var_type,
            "range": range_val,
            "options": options_val,
            "description": description
        })

if json_output:
    print(json.dumps(params, indent=2))
else:
    print(f"Parameters in: {filename}")
    print("=" * 55)
    print(f"{'NAME':<20} {'VALUE':<15} {'TYPE':<10} CONSTRAINT/DESC")
    print("-" * 55)
    for p in params:
        constraint = ""
        if p["range"]:
            constraint = f"[{p['range']}]"
        elif p["options"]:
            constraint = f"[{p['options']}]"
        if p["description"]:
            constraint = (constraint + " " + p["description"]).strip()
        print(f"{p['name']:<20} {p['value']:<15} {p['type']:<10} {constraint}")
'@

$tmpScript = [System.IO.Path]::GetTempFileName() + ".py"
try {
    Set-Content -Path $tmpScript -Value $pythonScript -Encoding UTF8

    if ($Json) {
        python $tmpScript $InputFile "--json"
    } else {
        python $tmpScript $InputFile
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Parameter extraction failed (is Python installed?)."
        exit 1
    }
} finally {
    if (Test-Path $tmpScript) { Remove-Item $tmpScript -ErrorAction SilentlyContinue }
}
