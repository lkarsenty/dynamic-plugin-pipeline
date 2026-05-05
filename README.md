# Dynamic Plugin Pipeline

![CI](https://github.com/<your-github-username>/<your-repo-name>/actions/workflows/ci.yml/badge.svg)

A Linux systems-programming project that implements a configurable,
multi-threaded text-processing pipeline. Each processing stage is loaded at
runtime as a shared-object plugin, runs on its own worker thread, and passes
data to the next stage through a bounded producer-consumer queue.

This project was developed and tested in a Linux container for an operating
systems course. It intentionally uses Linux/glibc dynamic-loader APIs such as
`dlmopen`, so the recommended way to build and run it is inside Linux or a
Linux container.

## Portfolio Summary

Built a configurable C plugin pipeline that dynamically loads shared libraries
at runtime and runs each plugin on its own worker thread. Implemented bounded
producer-consumer queues, plugin-to-plugin function-pointer dispatch, and
graceful shutdown coordination with pthread synchronization primitives.

## What It Demonstrates

- Dynamic loading of `.so` plugins with `dlmopen`, `dlsym`, and `dlclose`
- A small C plugin SDK based on function pointers
- Multi-threaded pipeline execution with `pthread`
- Bounded blocking queues using mutexes and condition variables
- Graceful shutdown propagation with a `<END>` sentinel
- Reusable plugin infrastructure shared by multiple transformations
- Automated shell tests for valid flows, invalid arguments, edge cases, and
  stress scenarios

## Architecture

Input enters the first plugin in the chain. Each plugin owns a queue and a
consumer thread:

```text
stdin
  |
  v
plugin 1 queue -> plugin 1 worker -> plugin 2 queue -> plugin 2 worker -> ...
```

The main program is responsible for:

1. Parsing the queue size and requested plugin names.
2. Loading each plugin from `output/<plugin>.so`.
3. Resolving the required SDK functions.
4. Initializing every plugin with the configured queue size.
5. Connecting each plugin to the next plugin in the chain.
6. Feeding input lines into the first plugin.
7. Waiting for all plugins to finish and cleaning up resources.

Each plugin implements a transformation function and reuses the common plugin
runtime in `plugins/plugin_common.c`.

## Available Plugins

| Plugin | Behavior |
| --- | --- |
| `logger` | Prints each string and forwards it unchanged |
| `typewriter` | Prints each string character-by-character with a delay |
| `uppercaser` | Converts lowercase ASCII letters to uppercase |
| `rotator` | Rotates the string one character to the right |
| `flipper` | Reverses the string |
| `expander` | Inserts spaces between characters |
| `prefixer` | Prepends `[prefix] ` to each string |

## Requirements

- Linux with glibc
- `gcc` or `gcc-13`
- `pthread`
- `libdl`
- `bash`

On macOS, the build will fail because `dlmopen` and `LM_ID_NEWLM` are not
provided by the macOS dynamic loader. Use the Docker workflow below or another
Linux environment.

## Build

```bash
./build.sh
```

Build artifacts are written to `output/`:

- `output/analyzer`
- `output/logger.so`
- `output/uppercaser.so`
- `output/rotator.so`
- `output/flipper.so`
- `output/expander.so`
- `output/typewriter.so`

To clean generated artifacts:

```bash
./build.sh clean
```

## Run

```bash
printf "hello\n<END>\n" | ./output/analyzer 5 uppercaser rotator logger
```

Example output:

```text
[logger] OHELL
Pipeline shutdown complete
```

You can compose plugins in different orders:

```bash
printf "abcd\n<END>\n" | ./output/analyzer 5 flipper uppercaser logger
```

```text
[logger] DCBA
Pipeline shutdown complete
```

The example `prefixer` plugin can be used like this:

```bash
printf "hello\n<END>\n" | ./output/analyzer 5 prefixer uppercaser logger
```

```text
[logger] [PREFIX] HELLO
Pipeline shutdown complete
```

## Test

```bash
./test.sh
```

The test script builds the project and checks:

- individual plugin behavior
- chained plugin behavior
- invalid command-line arguments
- invalid plugin names
- empty and long strings
- small queue sizes that force blocking
- repeated plugins in the same pipeline

## Docker Workflow

If you are working from macOS or another non-Linux host, run the project inside
a Linux container:

```bash
docker run --rm -it \
  -v "$PWD":/workspace \
  -w /workspace \
  gcc:13 \
  bash
```

Inside the container:

```bash
./test.sh
```

You can also build a project image and run the tests directly:

```bash
docker build -t dynamic-plugin-pipeline .
docker run --rm dynamic-plugin-pipeline
```

## Continuous Integration

The repository includes a GitHub Actions workflow in `.github/workflows/ci.yml`.
On every push and pull request, it runs the project on `ubuntu-latest`:

1. Checks out the repository.
2. Builds the analyzer and plugins with `./build.sh`.
3. Runs the end-to-end test suite with `./test.sh`.

After you publish the project to GitHub, replace the placeholder badge URL at
the top of this README with your actual GitHub username and repository name.

## Adding A Plugin

Plugins live in `plugins/*.c`. The build script automatically compiles each
plugin source file into `output/<plugin-name>.so`, so adding a plugin usually
only requires one new `.c` file.

Each plugin should:

1. Include `plugin_common.h`.
2. Export `plugin_get_name`.
3. Implement a transformation function that receives a string and returns a
   newly allocated string.
4. Export `plugin_init` and call `common_plugin_init`.

Minimal example:

```c
#include <stdlib.h>
#include <string.h>

#include "plugin_common.h"

__attribute__((visibility("default")))
const char* plugin_get_name(void) {
    return "prefixer";
}

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;

    const char* prefix = "[prefix] ";
    size_t prefix_len = strlen(prefix);
    size_t input_len = strlen(input);

    char* output = malloc(prefix_len + input_len + 1);
    if (!output) return NULL;

    memcpy(output, prefix, prefix_len);
    memcpy(output + prefix_len, input, input_len + 1);
    return output;
}

__attribute__((visibility("default")))
const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "prefixer", queue_size);
}
```

Then build and run it by name:

```bash
./build.sh
printf "hello\n<END>\n" | ./output/analyzer 5 prefixer logger
```

## Project Structure

```text
.
|-- main.c                         # Loads plugins and coordinates the pipeline
|-- build.sh                       # Builds analyzer and plugin .so files
|-- test.sh                        # End-to-end test script
|-- Dockerfile                     # Linux build/test environment
|-- .github/workflows/ci.yml       # GitHub Actions build/test workflow
`-- plugins
    |-- plugin_sdk.h               # Public plugin interface
    |-- plugin_common.c/.h         # Shared plugin runtime
    |-- logger.c                   # Example logging plugin
    |-- prefixer.c                 # Example custom plugin
    |-- uppercaser.c               # Uppercase transformation
    |-- rotator.c                  # Rotate transformation
    |-- flipper.c                  # Reverse transformation
    |-- expander.c                 # Space expansion transformation
    |-- typewriter.c               # Delayed output plugin
    `-- sync
        |-- monitor.c/.h           # Monitor abstraction
        `-- consumer_producer.c/.h # Bounded blocking queue
```

## Notes For Reviewers

The interesting part of this project is not the text transformations
themselves; they are intentionally simple. The focus is the runtime system
around them: dynamic loading, plugin isolation, thread coordination, bounded
queues, and orderly shutdown of a configurable pipeline.
