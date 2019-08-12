# System-wide config file for aewm clients

cmd "XTerm" "xterm"
cmd "Firefox" "firefox"
cmd "Mutt" "xterm -e mutt -y"

menu "Etc"
    cmd "The GIMP" "gimp"
    cmd "ELinks" "xterm -e elinks"
    cmd "Gaim" "gaim"
end

cmd "Logout" "skill aesession"
