#!/bin/bash
# Bash completion script for cmdgpt
# 
# Installation:
# - Source this file: source cmdgpt-completion.bash
# - Or add to ~/.bashrc: echo "source /path/to/cmdgpt-completion.bash" >> ~/.bashrc

_cmdgpt_completions()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    
    # Basic options
    opts="-h --help -v --version -i --interactive --stream -f --format -k --api_key -s --sys_prompt -l --log_file -m --model --log_level"
    
    # Handle option-specific completions
    case "${prev}" in
        -f|--format)
            COMPREPLY=( $(compgen -W "plain markdown json code" -- ${cur}) )
            return 0
            ;;
        -m|--model)
            COMPREPLY=( $(compgen -W "gpt-4 gpt-4-turbo gpt-3.5-turbo" -- ${cur}) )
            return 0
            ;;
        --log_level)
            COMPREPLY=( $(compgen -W "trace debug info warn error critical" -- ${cur}) )
            return 0
            ;;
        -l|--log_file)
            # File completion
            COMPREPLY=( $(compgen -f -- ${cur}) )
            return 0
            ;;
        -k|--api_key|-s|--sys_prompt)
            # No completion for these sensitive options
            return 0
            ;;
    esac
    
    # Complete options if current word starts with -
    if [[ ${cur} == -* ]] ; then
        COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
        return 0
    fi
}

# Register the completion function
complete -F _cmdgpt_completions cmdgpt