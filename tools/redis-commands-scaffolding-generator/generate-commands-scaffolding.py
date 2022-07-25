from pathlib import Path
from os import path
import argparse
import json
import logging


class Program:
    def __init__(self):
        self._setup_argument_parser()

    def _setup_argument_parser(self):
        self._parser = argparse.ArgumentParser()
        self._parser.add_argument(
            "--commands-json-path",
            required=True,
            type=str,
            help="Path to the json containing the commands data")
        self._parser.add_argument(
            "--commands-scaffolding-path",
            required=True,
            type=str,
            help="Path to the folder where to generate the commands scaffolding")

    def _parse_arguments(self):
        self._arguments = self._parser.parse_args()

        if self._arguments.commands_scaffolding_path.endswith("/"):
            self._arguments.commands_scaffolding_path = self._arguments.commands_scaffolding_path[:-1]

    def _ensure_output_folders_exists(self):
        Path(self._arguments.commands_scaffolding_path).mkdir(
            parents=True,
            exist_ok=True)

    def _generate_commands_scaffolding_header_file_command_context_fields(self, command_info: dict, argument: dict,
                                                                       start_indentation: int) -> str:
        lines = []
        field_type = None

        # Identify the type of the field
        if argument["type"] == "key":
            field_type = "module_redis_key_t"
        elif argument["type"] == "string":
            field_type = "storage_db_entry_chunk_sequence_t"
        elif argument["type"] == "integer" or argument["type"] == "unixtime":
            field_type = "int"
        elif argument["type"] == "bool":
            field_type = "bool"
        elif argument["type"] == "double":
            field_type = "double"
        elif argument["type"] == "pattern":
            field_type = "module_redis_pattern_t"
        elif argument["type"] in ["block", "oneof"]:
            command_context_struct_name = self._generate_command_context_struct_name(command_info)
            field_type = "{command_context_struct_name}_{struct_type}_argument_{name}_t".format(
                command_context_struct_name=command_context_struct_name,
                struct_type="block" if argument["type"] == "block" else "oneof",
                name=argument["name"])

        if field_type is None:
            raise Exception("field_type is none")

        if argument["has_multiple_occurrences"]:
            lines.append("{field_type} *list".format(field_type=field_type))
            lines.append("int count")
            lines.append("int current_entry")
        else:
            lines.append("{field_type} value".format(field_type=field_type))

        if not argument["token"] is None and argument["type"] != "pure-token":
            lines.append("bool has_token")

        start_indentation_str = " " * start_indentation
        header = \
            "{}struct {{\n{}    ".format(start_indentation_str, start_indentation_str) + \
            (";\n{}    ".format(start_indentation_str)).join(lines) + \
            ";\n{}}} {name};\n".format(start_indentation_str, name=argument["name"].replace("-", "_"))

        return header

    @staticmethod
    def _generate_command_context_struct_name(command_info: dict) -> str:
        return "module_redis_command_{command_callback_name}_context".format(**command_info)

    def _generate_command_context_typedef_name(self, command_info: dict) -> str:
        return "{}_t".format(self._generate_command_context_struct_name(command_info))

    def _generate_commands_scaffolding_header_file_command_context_struct(self, command_info: dict) -> str:
        header = ""

        command_context_struct_name = self._generate_command_context_struct_name(command_info)
        command_context_typedef_name = self._generate_command_context_typedef_name(command_info)

        has_block_arguments = False
        if len(command_info["arguments"]) > 0:
            for argument_index, argument in enumerate(command_info["arguments"]):
                if argument["type"] not in ["block", "oneof"]:
                    continue
                has_block_arguments = True

                struct_type = "block" if argument["type"] == "block" else "oneof"
                command_context_subargument_struct_name = \
                    "{command_context_struct_name}_{struct_type}_argument_{name}".format(
                        command_context_struct_name=command_context_struct_name,
                        struct_type=struct_type,
                        name=argument["name"])
                command_context_subargument_typedef_name = "{}_t".format(command_context_subargument_struct_name)

                header += "typedef struct {} {};\n".format(
                    command_context_subargument_struct_name, command_context_subargument_typedef_name)
                header += "struct {} {{\n".format(command_context_subargument_struct_name)

                for block_argument_index, block_argument in enumerate(argument["sub_arguments"]):
                    header += self._generate_commands_scaffolding_header_file_command_context_fields(
                        command_info=command_info, argument=block_argument, start_indentation=4)

                header += "};\n"

        if has_block_arguments:
            header += "\n"

        # Header of the struct
        header += "typedef struct {} {};\n".format(command_context_struct_name, command_context_typedef_name)
        header += "struct {} {{\n".format(command_context_struct_name)

        # Standard error messages related arguments
        header += "    char *error_message;\n"
        header += "    bool has_error;\n"

        # If there are no arguments, everything needed is just the error_message and the has_error properties
        if len(command_info["arguments"]) > 0:
            for argument_index, argument in enumerate(command_info["arguments"]):
                header += self._generate_commands_scaffolding_header_file_command_context_fields(
                    command_info=command_info, argument=argument, start_indentation=4)

        # Close the struct
        header += "}};\n".format(command_context_struct_name)

        return header

    @staticmethod
    def _generate_commands_scaffolding_header_file_command_key_specs_struct_table(command_info: dict) -> str:
        header = ""

        header += "module_redis_command_key_spec_t " \
                  "module_redis_command_{command_callback_name}_key_specs[] = {{\n".format(**command_info)

        for key_spec_index, key_spec in enumerate(command_info["key_specs"]):
            if len(key_spec["key_access_flags"]) > 0:
                key_access_flags = " | ".join([
                    "MODULE_REDIS_COMMAND_KEY_ACCESS_FLAGS_{}".format(flag)
                    for flag in key_spec["key_access_flags"]
                ])
            else:
                key_access_flags = 0

            if len(key_spec["value_access_flags"]) > 0:
                value_access_flags = " | ".join([
                    "MODULE_REDIS_COMMAND_VALUE_ACCESS_FLAGS_{}".format(flag)
                    for flag in key_spec["value_access_flags"]
                ])
            else:
                value_access_flags = 0

            header += (
                "    {\n        " +
                ",\n        ".join([
                    ".key_access_flags = {key_access_flags}",
                    ".value_access_flags = {value_access_flags}",
                    ".is_unknown = {is_unknown}",
                    ".begin_search_index_pos = {begin_search_index_pos}",
                    ".find_keys_range_lastkey = {find_keys_range_lastkey}",
                    ".find_keys_range_step = {find_keys_range_step}",
                    ".find_keys_range_limit = {begin_search_index_pos}",
                ]).format(
                    key_access_flags=key_access_flags,
                    value_access_flags=value_access_flags,
                    is_unknown="true" if key_spec["is_unknown"] else "false",
                    begin_search_index_pos=key_spec["begin_search_index_pos"],
                    find_keys_range_lastkey=key_spec["find_keys_range_lastkey"],
                    find_keys_range_step=key_spec["find_keys_range_step"],
                    find_keys_range_limit=key_spec["find_keys_range_limit"],
                ) +
                "\n    }"
            )

            if key_spec_index + 1 < len(command_info["key_specs"]):
                header += ","
            header += "\n"

        header += "};\n"

        return header

    @staticmethod
    def _generate_commands_scaffolding_header_file_argument_structs_content(arguments) -> str:
        return ",\n".join([
                "    {{\n        " +
                ",\n        ".join([
                    ".name = \"{name}\"",
                    ".type = MODULE_REDIS_COMMAND_ARGUMENT_TYPE_{type}",
                    ".since = \"{since}\"",
                    ".key_spec_index = {key_spec_index}",
                    # Token can be both NULL or a string, the quoting is taking care in the format below
                    ".token = {token}",
                    ".is_positional = {is_positional}",
                    ".is_optional = {is_optional}",
                    ".is_sub_argument = {is_sub_argument}",
                    ".has_sub_arguments = {has_sub_arguments}",
                    ".has_multiple_occurrences = {has_multiple_occurrences}",
                    ".has_multiple_token = {has_multiple_token}",
                ]).format(
                    name=argument["name"].replace("-", "_").lower(),
                    type=argument["type"].replace("-", "").upper(),
                    since=argument["since"],
                    key_spec_index=(
                        argument["key_spec_index"]
                        if not argument["key_spec_index"] is None else -1),
                    token=(
                        "\"" + argument["token"] + "\""
                        if not argument["token"] is None else "NULL"),
                    is_optional="true" if argument["is_optional"] else "false",
                    is_positional="true" if argument["is_positional"] else "false",
                    is_sub_argument="true" if argument["is_sub_argument"] else "false",
                    has_sub_arguments="true" if argument["has_sub_arguments"] else "false",
                    has_multiple_occurrences="true" if argument["has_multiple_occurrences"] else "false",
                    has_multiple_token="true" if argument["has_multiple_token"] else "false",
                ) +
                "\n    }}"
                for argument in arguments
            ]
        )

    def _generate_commands_scaffolding_header_file_command_sub_arguments_structs_tables(
            self, command_info: dict) -> str:
        header = ""

        for argument_index, argument in enumerate(command_info["arguments"]):
            if len(argument["sub_arguments"]) == 0:
                continue

            header += (
                "module_redis_command_argument_t "
                "module_redis_command_{command_callback_name}_argument_{argument_index}_sub_arguments[] = {{\n" +
                self._generate_commands_scaffolding_header_file_argument_structs_content(
                    argument["sub_arguments"]) + "\n" +
                "}};\n"
            ).format(**command_info, argument_index=argument_index)

        return header

    def _generate_commands_scaffolding_header_file_command_arguments_structs_table(self, command_info: dict) -> str:
        return (
                "module_redis_command_argument_t module_redis_command_{command_callback_name}_arguments[] = {{\n" +
                self._generate_commands_scaffolding_header_file_argument_structs_content(
                    command_info["arguments"]) + "\n" +
                "}};\n"
        ).format(**command_info)

    @staticmethod
    def _generate_commands_scaffolding_commands_header_command_callback(command_info: dict) -> str:
        return (
            "\n".join([
                "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM_AUTOGEN({command_callback_name});",
                "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN_AUTOGEN({command_callback_name});",
                "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA_AUTOGEN({command_callback_name});",
                "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END_AUTOGEN({command_callback_name});",
                "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL_AUTOGEN({command_callback_name});",
                "MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END({command_callback_name});",
            ]).format(
                command_string=command_info["command_string"],
                command_callback_name=command_info["command_callback_name"]) + "\n"
        )

    @staticmethod
    def _generate_commands_scaffolding_header_file_command_commands_struct_header(command_info: dict) -> str:
        required_arguments_count = sum([
            0 if argument["is_optional"] else 1
            for argument in command_info["arguments"]
        ])

        return "    MODULE_REDIS_COMMAND_AUTOGEN(" \
               "{command_callback_name_uppercase}, " \
               "\"{command_string}\", " \
               "{command_callback_name}, " \
               "{required_arguments_count}, " \
               "{arguments_count}, " \
               "{key_specs_count}" \
               "),".format(
                    **command_info,
                    command_callback_name_uppercase=command_info["command_callback_name"].upper(),
                    required_arguments_count=required_arguments_count,
                    arguments_count=len(command_info["arguments"]),
                    key_specs_count=len(command_info["key_specs"]))

    @staticmethod
    def _write_header_header(fp, header_name):
        fp.writelines([
            "//\n",
            "// DO NOT EDIT - AUTO GENERATED\n",
            "//\n",
            "\n"
            "#ifndef {header_name}\n".format(header_name=header_name),
            "#define {header_name}\n".format(header_name=header_name),
            "\n",
            "#ifdef __cplusplus\n",
            "extern \"C\" {\n",
            "#endif\n"
            "\n"
        ])

    @staticmethod
    def _write_header_footer(fp, header_name):
        fp.writelines([
            "\n",
            "#ifdef __cplusplus\n",
            "}\n",
            "#endif\n",
            "\n",
            "#endif //{header_name}\n".format(header_name=header_name),
        ])

    def _generate_commands_module_redis_autogenerated_commands_enum_h(self, commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path,
                "module_redis_autogenerated_commands_enum.h"), "w") as fp:
            self._write_header_header(
                fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_ENUM_H")

            fp.writelines([
                "enum module_redis_commands {\n"
                "    MODULE_REDIS_COMMAND_NOP = 0,\n"
                "    MODULE_REDIS_COMMAND_UNKNOWN,\n"
            ])

            for command_info in commands_info:
                fp.writelines([
                    "    MODULE_REDIS_COMMAND_{command_string},\n".format(
                        command_string=command_info["command_string"]),
                ])

            fp.writelines([
                "};", "\n",
                "typedef enum module_redis_commands module_redis_commands_t;", "\n",
            ])

            self._write_header_footer(
                fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_ENUM_H")

    def _generate_commands_module_redis_autogenerated_commands_callbacks_h(self, commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path,
                "module_redis_autogenerated_commands_callbacks.h"), "w") as fp:
            self._write_header_header(
                fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_CALLBACKS_H")

            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_commands_header_command_callback(command_info), "\n",
                ])

            self._write_header_footer(
                fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_CALLBACKS_H")

    def _generate_commands_module_redis_autogenerated_commands_table_h_header(self, commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path, "module_redis_autogenerated_commands_table.h"), "w") as fp:

            self._write_header_header(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_TABLE_H")

            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_header_file_command_key_specs_struct_table(command_info),
                    self._generate_commands_scaffolding_header_file_command_sub_arguments_structs_tables(
                        command_info),
                    self._generate_commands_scaffolding_header_file_command_arguments_structs_table(command_info),
                    "\n",
                ])

            fp.writelines(["module_redis_command_info_t command_infos_map[] = {\n"])
            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_header_file_command_commands_struct_header(command_info), "\n"
                ])
            fp.writelines(["};\n"])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_TABLE_H")

    def _generate_commands_module_redis_command_autogenerated_c_headers_general(self, fp):
        fp.writelines("\n".join([
            "#include <stdint.h>",
            "#include <stdbool.h>",
            "#include <string.h>",
            "#include <strings.h>",
            "#include <arpa/inet.h>",
            "",
            "#include \"misc.h\"",
            "#include \"exttypes.h\"",
            "#include \"log/log.h\"",
            "#include \"clock.h\"",
            "#include \"spinlock.h\"",
            "#include \"data_structures/small_circular_queue/small_circular_queue.h\"",
            "#include \"data_structures/double_linked_list/double_linked_list.h\"",
            "#include \"data_structures/queue_mpmc/queue_mpmc.h\"",
            "#include \"slab_allocator.h\"",
            "#include \"data_structures/hashtable/mcmp/hashtable.h\"",
            "#include \"protocol/redis/protocol_redis.h\"",
            "#include \"protocol/redis/protocol_redis_reader.h\"",
            "#include \"protocol/redis/protocol_redis_writer.h\"",
            "#include \"modules/module.h\"",
            "#include \"network/io/network_io_common.h\"",
            "#include \"config.h\"",
            "#include \"fiber.h\"",
            "#include \"network/channel/network_channel.h\"",
            "#include \"storage/io/storage_io_common.h\"",
            "#include \"storage/channel/storage_channel.h\"",
            "#include \"storage/db/storage_db.h\"",
            "#include \"network/network.h\"",
            "#include \"worker/worker_stats.h\"",
            "#include \"worker/worker_context.h\"",
            "#include \"modules/redis/module_redis.h\"",
            "#include \"module_redis_autogenerated_commands_contexts.h\"",
            "#include \"module_redis_autogenerated_commands_callbacks.h\"",
            "",
            "",
        ]) + "\n")

    def _generate_commands_module_redis_command_autogenerated_tag_name(self, command_info: dict) -> str:
        return "TAG_{command_callback_name}".format(command_callback_name=command_info["command_callback_name"])

    def _generate_commands_module_redis_command_autogenerated_c_headers(self, command_info: dict) -> str:
        return "\n".join([
            "//",
            "// COMMAND {command_string}",
            "//",
            "#define {tag_name} \"module_redis_command_{command_callback_name}\"",
        ]).format(
            command_string=command_info["command_string"],
            command_callback_name=command_info["command_callback_name"],
            tag_name=self._generate_commands_module_redis_command_autogenerated_tag_name(
                command_info=command_info)) + "\n"

    def _generate_commands_module_redis_command_autogenerated_c_argument_require_stream(self, command_info: dict) \
            -> str:
        return "\n".join([
            "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_REQUIRE_STREAM_AUTOGEN({command_callback_name}) {{",
            "    return false;",
            "}}",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info)) + "\n"

    def _generate_commands_module_redis_command_autogenerated_c_argument_full(self, command_info: dict) -> str:
        return "\n".join([
            "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_FULL_AUTOGEN({command_callback_name}) {{",
            "    return true;",
            "}}",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info)) + "\n"

    def _generate_commands_module_redis_command_autogenerated_c_argument_stream_begin(self, command_info: dict) -> str:
        return "\n".join([
            "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_BEGIN_AUTOGEN({command_callback_name}) {{",
            "    return true;",
            "}}",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info)) + "\n"

    def _generate_commands_module_redis_command_autogenerated_c_argument_stream_data(self, command_info: dict) -> str:
        return "\n".join([
            "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_DATA_AUTOGEN({command_callback_name}) {{",
            "    return true;",
            "}}",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info)) + "\n"

    def _generate_commands_module_redis_command_autogenerated_c_argument_stream_end(self, command_info: dict) -> str:
        return "\n".join([
            "MODULE_REDIS_COMMAND_FUNCPTR_ARGUMENT_STREAM_END_AUTOGEN({command_callback_name}) {{",
            "    return true;",
            "}}",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info)) + "\n"

    def _generate_commands_module_redis_command_autogenerated_c_command_end(self, command_info: dict) -> str:
        return "\n".join([
            "#ifndef MODULE_REDIS_COMMAND_{command_callback_name_uppercase}_CALLBACK_PROVIDED",
            "MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END({command_callback_name}) {{",
            "    bool return_res = false;",
            "    char error_message[] = \"Command not implemented!\";",
            "    network_channel_buffer_data_t *send_buffer, *send_buffer_start, *send_buffer_end;",
            "",
            "    size_t slice_length = sizeof(error_message) + 16;",
            "    send_buffer = send_buffer_start = network_send_buffer_acquire_slice(channel, slice_length);",
            "    if (send_buffer_start == NULL) {{",
            "        LOG_E({tag_name}, \"Unable to acquire send buffer slice!\");",
            "        goto end;",
            "    }}",
            "",
            "    send_buffer_start = protocol_redis_writer_write_simple_error(",
            "            send_buffer_start,",
            "            slice_length,",
            "            error_message,",
            "            (int)strlen(error_message));",
            "    network_send_buffer_release_slice(",
            "            channel,",
            "            send_buffer_start ? send_buffer_start - send_buffer : 0);",
            "",
            "    return_res = send_buffer_start != NULL;",
            "",
            "    if (send_buffer_start == NULL) {{",
            "        LOG_E({tag_name}, \"buffer length incorrectly calculated, not enough space!\");",
            "    }}",
            "",
            "    goto end;",
            "",
            "    return_res = true;",
            "",
            "end:",
            "    return return_res;",
            "}}",
            "#endif",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_callback_name_uppercase=command_info["command_callback_name"].upper(),
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info),
            tag_name=self._generate_commands_module_redis_command_autogenerated_tag_name(
                command_info=command_info)) + "\n"

    def _generate_commands_module_redis_autogenerated_commands_callbacks_c(self, fp, command_info: dict):
        # TODO: when support for special types will be added it will be necessary to check if extra headers will
        #       be needed, e.g. for the hashtables, sets, lists extra needed will certainly have to be added

        fp.writelines([
            self._generate_commands_module_redis_command_autogenerated_c_headers(command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_argument_require_stream(
                command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_argument_full(command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_argument_stream_begin(command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_argument_stream_data(command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_argument_stream_end(command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_command_end(command_info), "\n",
        ])

    def _generate_commands_module_redis_autogenerated_commands_contexts_h_header(self, commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path,
                "module_redis_autogenerated_commands_contexts.h"), "w") as fp:

            self._write_header_header(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMAND_CONTEXTS_H")

            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_header_file_command_context_struct(command_info), "\n",
                ])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMAND_CONTEXTS_H")

    def _generate_commands_module_redis_autogenerated_commands_callbacks_c_code(self, commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path,
                "module_redis_autogenerated_commands_callbacks.c"), "w") as fp_commands_callbacks:

            self._generate_commands_module_redis_command_autogenerated_c_headers_general(fp_commands_callbacks)

            for command_info in commands_info:
                self._generate_commands_module_redis_autogenerated_commands_callbacks_c(
                    fp=fp_commands_callbacks, command_info=command_info)

    def _generate_commands_scaffolding(self, commands_info: list):
        self._generate_commands_module_redis_autogenerated_commands_callbacks_h(commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_enum_h(commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_table_h_header(commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_contexts_h_header(commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_callbacks_c_code(commands_info=commands_info)

    def main(self):
        logging.basicConfig(
            level=logging.INFO,
            format='[%(asctime)s][%(levelname)s] %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S')

        try:
            self._parse_arguments()
            self._ensure_output_folders_exists()

            # Fetch the commands from the json specs file
            with open(self._arguments.commands_json_path, "r") as fp:
                commands_info = json.load(fp)

            self._generate_commands_scaffolding(commands_info)

        except Exception as e:
            logging.error(str(e), exc_info=True)


if __name__ == "__main__":
    Program().main()
