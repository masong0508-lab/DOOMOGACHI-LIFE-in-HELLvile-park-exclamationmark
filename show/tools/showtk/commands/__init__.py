"""showtk.commands - one module per `show` subcommand (or family).

Add a file <name>.py here that defines register(subparsers); cli.py
imports it automatically. Keep the module cheap to import: heavy imports
(numpy, onnxruntime, ...) go inside the command's run function.
"""
