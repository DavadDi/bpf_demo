# bee

# bee --help
Usage:
  bee [command]

Available Commands:
  build       Build a BPF program, and save it to an OCI image representation.
  completion  generate the autocompletion script for the specified shell
  describe    Describe a BPF program via it's OCI ref
  help        Help about any command
  init        Initialize a sample BPF program
  list
  login       Log in so you can push images to the remote server.
  pull
  push
  run         Run a BPF program file or OCI image.
  tag
  version

Flags:
  -c, --config stringArray   path to auth configs
      --config-dir string    Directory to bumblebee configuration (default "/root/.bumblebee")
  -h, --help                 help for bee
      --insecure             allow connections to SSL registry without certs
  -p, --password string      registry password
      --plain-http           use plain http and not https
      --storage string       Directory to store OCI images locally (default "/root/.bumblebee/store")
  -u, --username string      registry username
  -v, --verbose              verbose output

Use "bee [command] --help" for more information about a command.
