# Bash completion for scram.
# Run 'source scram.sh' or put this file in /etc/bash_completion.d/

########################################
# Searches for short and long options
# starting with "-" and prints them
#
# Globals:
#   None
# Arguments:
#   A line containing options
# Returns:
#   Short option
#   Long option
########################################
_scram_parse_options() {
  local short_option long_option w
  for w in $1; do
    case "${w}" in
      -?) [[ -n "${short_option}" ]] || short_option="${w}" ;;
      --*) [[ -n "${long_option}" ]] || long_option="${w}" ;;
    esac
  done

  [[ -n "${short_option}" ]] && printf "%s\n" "${short_option}"
  [[ -n "${long_option}" ]] && printf "%s\n" "${long_option}"
}

########################################
# Parses SCRAM's Boost program options
# generated help description to find
# options
#
# Globals:
#   None
# Arguments:
#   None
# Returns:
#   Options
########################################
_scram_parse_help() {
  local line
  scram --help | while read -r line; do
    [[ "${line}" == *-* ]] || continue
    _scram_parse_options "${line}"
  done
}

########################################
# Bash completion function for SCRAM
#
# Globals:
#   COMPREPLY
# Arguments:
#   None
# Returns:
#   Completion suggestions
########################################
_scram() {
  local cur prev type
  _init_completion -n = || return

  case "${prev}" in
    --verbosity)
      COMPREPLY=($(compgen -W "0 1 2 3 4 5 6 7" -- "${cur}"))
      return
      ;;
    -*)
      type=$(scram --help | \
        grep -Poi "^\s*(${prev}|\-\w \[ ${prev} \]|${prev} \[ --\S+ \])\s\K\w+")
      case "${type}" in
        path)
          _filedir
          return
          ;;
        bool)
          COMPREPLY=($(compgen -W "on off yes no true false 1 0" -- "${cur}"))
          return
          ;;
        double|int)
          # An argument is required.
          return
          ;;
      esac
  esac

  # Start parsing the help for option suggestions.
  if [[ "${cur}" == -* ]]; then
    COMPREPLY=($(compgen -W "$(_scram_parse_help)" -- "${cur}"))
    [[ -n "${COMPREPLY}" ]] && return
  fi

  # Default to input files.
  _filedir
}

complete -F _scram scram
