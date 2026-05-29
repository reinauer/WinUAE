#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
	echo "usage: $0 /path/to/winuae_unix" >&2
	exit 2
fi

catalog="$("$1" --qt-board-catalog)"

require_entry()
{
	key="$1"
	name="$2"
	if ! printf '%s\n' "$catalog" | awk -F '	' -v key="$key" '
		$1 == "E" && $2 == key { found = 1 }
		END { exit found ? 0 : 1 }
	'; then
		echo "missing sound-board catalog entry: $name" >&2
		exit 1
	fi
}

require_category()
{
	key="$1"
	name="$2"
	category="$3"
	if ! printf '%s\n' "$catalog" | awk -F '	' -v key="$key" -v category="$category" '
		$1 == "E" && $2 == key && $5 == category { found = 1 }
		END { exit found ? 0 : 1 }
	'; then
		echo "catalog entry has wrong category: $name" >&2
		exit 1
	fi
}

require_option()
{
	key="$1"
	name="$2"
	option="$3"
	if ! printf '%s\n' "$catalog" | awk -F '	' -v key="$key" -v option="$option" '
		$1 == "EO" && $2 == key && $4 == option { found = 1 }
		END { exit found ? 0 : 1 }
	'; then
		echo "missing sound-board option: $name / $option" >&2
		exit 1
	fi
}

sound_category=256
custom_category=16

require_entry cHJlbHVkZQ== "Prelude"
require_entry cHJlbHVkZTEyMDA= "Prelude 1200"
require_entry dG9jY2F0YQ== "Toccata"
require_entry ZXMxMzcw "ES1370 PCI"
require_entry Zm04MDE= "FM801 PCI"
require_entry dWFlc25kX3oy "UAESND Zorro II"
require_entry dWFlc25kX3oz "UAESND Zorro III"
require_entry dWFlYm9hcmRfejI= "UAEBOARD Zorro II"
require_entry dWFlYm9hcmRfejM= "UAEBOARD Zorro III"

require_category cHJlbHVkZQ== "Prelude" "$sound_category"
require_category cHJlbHVkZTEyMDA= "Prelude 1200" "$sound_category"
require_category dG9jY2F0YQ== "Toccata" "$sound_category"
require_category ZXMxMzcw "ES1370 PCI" "$sound_category"
require_category Zm04MDE= "FM801 PCI" "$sound_category"
require_category dWFlc25kX3oy "UAESND Zorro II" "$sound_category"
require_category dWFlc25kX3oz "UAESND Zorro III" "$sound_category"

# Windows exposes UAEBOARD in the Custom category, not Sound boards.
require_category dWFlYm9hcmRfejI= "UAEBOARD Zorro II" "$custom_category"
require_category dWFlYm9hcmRfejM= "UAEBOARD Zorro III" "$custom_category"

# Toccata-family cards need the shared Paula/CD mixer setting.
require_option cHJlbHVkZQ== "Prelude" bWl4ZXI=
require_option cHJlbHVkZTEyMDA= "Prelude 1200" bWl4ZXI=
require_option dG9jY2F0YQ== "Toccata" bWl4ZXI=
