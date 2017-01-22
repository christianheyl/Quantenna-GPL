# ~/.bashrc: executed by bash(1) for non-login interactive shells.

export PATH=\
/bin:\
/sbin:\
/usr/bin:\
/usr/sbin:\
/scripts:\
/usr/local/bin

# If running interactively, then:
if [ "$PS1" ]; then

    if [ "`id -u`" -eq 0 ]; then 
        export PS1='quantenna # '
    else
        export PS1='quantenna $ '
    fi

    export USER=`id -un`
    export LOGNAME=$USER
    export HOSTNAME=`/bin/hostname`
    export HISTSIZE=1000
    export HISTFILESIZE=1000
    export PAGER='/bin/more '
    export EDITOR='/bin/vi'
	export INPUTRC='/etc/inputrc'

    ### Some aliases
    alias ps2='ps facux '
    alias ps1='ps faxo "%U %t %p %a" '
    alias af='ps af'
    alias cls='clear'
    alias m='more'
    alias g='grep'
    alias ll='/bin/ls --color=tty -laFh'
    alias ls='/bin/ls --color=tty -F'
    alias df='df -h'
fi;

