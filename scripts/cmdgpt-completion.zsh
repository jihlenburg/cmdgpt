#compdef cmdgpt
# Zsh completion script for cmdgpt
#
# Installation:
# - Add to fpath: fpath+=(/path/to/directory/containing/this/file)
# - Or copy to: /usr/local/share/zsh/site-functions/_cmdgpt

_cmdgpt() {
    local -a options
    local -a formats
    local -a models
    local -a log_levels
    
    # Define option groups
    options=(
        '(-h --help)'{-h,--help}'[Show help message and exit]'
        '(-v --version)'{-v,--version}'[Print version and exit]'
        '(-i --interactive)'{-i,--interactive}'[Run in interactive mode (REPL)]'
        '--stream[Enable streaming responses]'
        '(-f --format)'{-f,--format}'[Output format]:format:->format'
        '(-k --api_key)'{-k,--api_key}'[OpenAI API key]:api_key:'
        '(-s --sys_prompt)'{-s,--sys_prompt}'[System prompt]:prompt:'
        '(-l --log_file)'{-l,--log_file}'[Log file path]:log_file:_files'
        '(-m --model)'{-m,--model}'[GPT model]:model:->model'
        '--log_level[Log level]:level:->log_level'
        '*:prompt:_default'
    )
    
    formats=(
        'plain:Plain text output'
        'markdown:Markdown formatted output'
        'json:JSON formatted output'
        'code:Extract code blocks only'
    )
    
    models=(
        'gpt-4:GPT-4 (most capable)'
        'gpt-4-turbo:GPT-4 Turbo (faster)'
        'gpt-3.5-turbo:GPT-3.5 Turbo (fastest)'
    )
    
    log_levels=(
        'trace:Most verbose logging'
        'debug:Debug information'
        'info:Informational messages'
        'warn:Warnings only'
        'error:Errors only'
        'critical:Critical errors only'
    )
    
    _arguments -s -S -C \
        "${options[@]}"
    
    case $state in
        format)
            _describe -t formats 'output format' formats
            ;;
        model)
            _describe -t models 'GPT model' models
            ;;
        log_level)
            _describe -t log_levels 'log level' log_levels
            ;;
    esac
}

_cmdgpt "$@"