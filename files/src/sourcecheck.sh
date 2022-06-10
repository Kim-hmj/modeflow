#stdbuf -oL grep -nr  -E "p\(\".*[\.\+\*].*\"," | grep -v libmodeflow
grep -nrE 'e.+_re\("[^\(]+\|.+' 
#not _re used
grep -nr Map\(\" | grep  '|'
grep -nr Map\(\" | grep  '\.+'

grep -Enr "*+\(\?\!.+\).+" |  grep -P "\(\?\![a-zA-Z2]+\)(\"|:)"
grep -nr Map\(\" | egrep  -v "::"
egrep -nr "Map[^:]+[^:]:[^:]"

grep -Enr [^:]source:[^:]{1}
grep -Enr [^:]devpwr:[^:]{1}
grep -Enr [^:]playerstate:[^:]{1}

