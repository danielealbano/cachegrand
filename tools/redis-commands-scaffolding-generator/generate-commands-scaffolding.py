from pathlib import Path
from os import path
from typing import Optional
import argparse
import json
import logging


class Program:
    def __init__(
            self):
        self._setup_argument_parser()

    def _setup_argument_parser(
            self):
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

    def _parse_arguments(
            self):
        self._arguments = self._parser.parse_args()

        if self._arguments.commands_scaffolding_path.endswith("/"):
            self._arguments.commands_scaffolding_path = self._arguments.commands_scaffolding_path[:-1]

    def _ensure_output_folders_exists(
            self):
        Path(self._arguments.commands_scaffolding_path).mkdir(
            parents=True,
            exist_ok=True)

    def _generate_commands_scaffolding_header_file_command_context_fields(
            self,
            command_info: dict,
            argument: dict,
            start_indentation: int,
            command_context_struct_name_suffix: str) -> str:
        lines = []
        field_type = None

        # Identify the type of the field
        if argument["type"] == "key":
            field_type = "module_redis_key_t"
        elif argument["type"] == "string":
            field_type = "storage_db_chunk_sequence_t*"
        elif argument["type"] == "integer":
            field_type = "int64_t"
        elif argument["type"] == "unixtime":
            field_type = "uint64_t"
        elif argument["type"] == "bool":
            field_type = "bool"
        elif argument["type"] == "double":
            field_type = "double"
        elif argument["type"] == "pattern":
            field_type = "module_redis_pattern_t"
        elif argument["type"] in ["block", "oneof"]:
            command_context_struct_name = self._generate_command_context_struct_name(command_info)
            field_type = \
                "{command_context_struct_name}" \
                "{command_context_struct_name_suffix}" \
                "_subargument_{argument_name}_t".format(
                    command_context_struct_name=command_context_struct_name,
                    command_context_struct_name_suffix=command_context_struct_name_suffix,
                    argument_name=argument["name"].replace("-", "_"))

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
    def _generate_command_context_struct_name(
            command_info: dict) -> str:
        return "module_redis_command_{command_callback_name}_context".format(**command_info)

    def _generate_command_context_typedef_name(
            self,
            command_info: dict) -> str:
        return "{}_t".format(self._generate_command_context_struct_name(command_info))

    def _generate_commands_scaffolding_header_file_command_context_struct(
            self,
            command_info: dict,
            arguments: list,
            command_context_struct_name_suffix: str) -> str:
        header = ""

        command_context_struct_name = self._generate_command_context_struct_name(command_info)

        has_sub_arguments = False
        for argument_index, argument in enumerate(arguments):
            if len(argument["sub_arguments"]) == 0:
                continue
            has_sub_arguments = True

            header += self._generate_commands_scaffolding_header_file_command_context_struct(
                command_info=command_info,
                arguments=argument["sub_arguments"],
                is_sub_argument=True,
                command_context_struct_name_suffix="{}_subargument_{}".format(
                    command_context_struct_name_suffix,
                    argument["name"].replace("-", "_")))

        if has_sub_arguments:
            header += "\n"

        # Header of the struct
        command_context_arguments_struct_name = "{}{}".format(
            command_context_struct_name,
            command_context_struct_name_suffix)

        header += "typedef struct {} {}_t;\n".format(
            command_context_arguments_struct_name, command_context_arguments_struct_name)
        header += "struct {} {{\n".format(command_context_arguments_struct_name)

        # If there are no arguments, everything needed is just the error_message and the has_error properties
        for argument_index, argument in enumerate(arguments):
            header += self._generate_commands_scaffolding_header_file_command_context_fields(
                command_info=command_info,
                argument=argument,
                start_indentation=4,
                command_context_struct_name_suffix=command_context_struct_name_suffix)

        # Close the struct
        header += "};\n"

        return header

    @staticmethod
    def _generate_commands_scaffolding_header_file_command_key_specs_struct_table(
            command_info: dict) -> str:
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
                key_access_flags = "MODULE_REDIS_COMMAND_KEY_ACCESS_FLAGS_UNKNOWN"

            if len(key_spec["value_access_flags"]) > 0:
                value_access_flags = " | ".join([
                    "MODULE_REDIS_COMMAND_VALUE_ACCESS_FLAGS_{}".format(flag)
                    for flag in key_spec["value_access_flags"]
                ])
            else:
                value_access_flags = "MODULE_REDIS_COMMAND_VALUE_ACCESS_FLAGS_UNKNOWN"

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

    def _generate_commands_scaffolding_header_file_argument_structs_content(
            self,
            command_info: dict,
            arguments: list,
            command_context_struct_name: str,
            argument_decl_name: str,
            parent_argument_index: Optional[int],
            parent_argument_decl_name: str) -> str:
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
                    ".argument_context_offset = offsetof({command_context_struct_name}_t,{command_context_field_name})"
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
                    command_context_struct_name=command_context_struct_name,
                    command_context_field_name=argument["name"].replace("-", "_"),
                ) +
                "\n    }}"
                for argument in arguments
            ]
        )

    def _generate_commands_scaffolding_header_file_command_arguments_structs_definitions(
            self,
            command_info: dict,
            arguments: list,
            argument_decl_name_suffix: str,
            command_context_struct_name: str) -> str:
        header = ""
        header = ""
        argument_decl_name = "module_redis_command_{command_callback_name}{argument_decl_name_suffix}".format(
            command_callback_name=command_info["command_callback_name"],
            argument_decl_name_suffix=argument_decl_name_suffix)

        for argument_index, argument in enumerate(arguments):
            if len(argument["sub_arguments"]) == 0:
                continue

            header += self._generate_commands_scaffolding_header_file_command_arguments_structs_table(
                command_info=command_info,
                arguments=argument["sub_arguments"],
                argument_decl_name_suffix="{argument_decl_name_suffix}_subargument_{argument_name}".format(
                    argument_decl_name_suffix=argument_decl_name_suffix,
                    argument_name=argument["name"].replace("-", "_")),
                command_context_struct_name="{command_context_struct_name}_subargument_{argument_name}".format(
                    command_context_struct_name=command_context_struct_name,
                    argument_name=argument["name"].replace("-", "_")))

        header += (
                "module_redis_command_argument_t {argument_decl_name}_arguments[] = {{\n" +
                self._generate_commands_scaffolding_header_file_argument_structs_content(
                    command_info=command_info,
                    arguments=arguments,
                    command_context_struct_name=command_context_struct_name) + "\n" +
                "}};\n"
        ).format(
            argument_decl_name=argument_decl_name)

        return header

    @staticmethod
    def _generate_commands_scaffolding_commands_header_command_callback(
            command_info: dict) -> str:
        return (
            "\n".join([
                "MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END({command_callback_name});",
                "MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_FREE_AUTOGEN({command_callback_name});",
            ]).format(
                command_string=command_info["command_string"],
                command_callback_name=command_info["command_callback_name"]) + "\n"
        )

    @staticmethod
    def _write_header_header(
            fp,
            header_name):
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
    def _write_header_footer(
            fp,
            header_name):
        fp.writelines([
            "\n",
            "#ifdef __cplusplus\n",
            "}\n",
            "#endif\n",
            "\n",
            "#endif //{header_name}\n".format(header_name=header_name),
        ])

    def _generate_commands_module_redis_autogenerated_commands_enum_h_header(
            self,
            commands_info: list):
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

    def _generate_commands_module_redis_autogenerated_commands_callbacks_h_header(
            self,
            commands_info: list):
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

    def _generate_commands_module_redis_autogenerated_commands_arguments_h_header(
            self,
            commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path, "module_redis_autogenerated_commands_arguments.h"), "w") as fp:

            self._write_header_header(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_ARGUMENTS_H")

            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_header_file_command_arguments_structs_table(
                        command_info=command_info,
                        arguments=command_info["arguments"],
                        argument_decl_name_suffix="",
                        command_context_struct_name=self._generate_command_context_struct_name(command_info)),
                    "\n",
                ])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_ARGUMENTS_H")

    def _generate_commands_module_redis_autogenerated_commands_key_specs_h_header(
            self,
            commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path, "module_redis_autogenerated_commands_key_specs.h"), "w") \
                as fp:

            self._write_header_header(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_KEY_SPECS_TABLE_H")

            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_header_file_command_key_specs_struct_table(command_info),
                    "\n",
                ])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_KEY_SPECS_TABLE_H")

    def _generate_commands_module_redis_autogenerated_commands_info_map_h_header(
            self,
            commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path, "module_redis_autogenerated_commands_info_map.h"), "w") \
                as fp:

            self._write_header_header(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_INFO_MAP_H")

            fp.writelines(["module_redis_command_info_t command_infos_map[] = {\n"])
            for command_info in commands_info:
                required_arguments_count = sum([
                    0 if argument["is_optional"] else 1
                    for argument in command_info["arguments"]
                ])


                fp.writelines([
                    "    MODULE_REDIS_COMMAND_AUTOGEN(" \
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
                        key_specs_count=len(command_info["key_specs"])),
                    "\n",
                ])

            fp.writelines(["};\n"])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_INFO_MAP_H")

    def _generate_commands_module_redis_command_autogenerated_c_headers_general(
            self,
            fp):
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
            "#include \"module/module.h\"",
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
            "#include \"module/redis/module_redis.h\"",
            "#include \"module/redis/module_redis_connection.h\"",
            "#include \"module/redis/module_redis_command.h\"",
            "#include \"module_redis_autogenerated_commands_contexts.h\"",
            "#include \"module_redis_autogenerated_commands_callbacks.h\"",
            "",
            "",
        ]) + "\n")

    def _generate_commands_module_redis_command_autogenerated_tag_name(
            self,
            command_info: dict) -> str:
        return "TAG_{command_callback_name}".format(command_callback_name=command_info["command_callback_name"])

    def _generate_commands_module_redis_command_autogenerated_c_headers(
            self,
            command_info: dict) -> str:
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

    def _generate_commands_module_redis_command_autogenerated_c_command_end(
            self,
            command_info: dict) -> str:
        return "\n".join([
            "#ifndef MODULE_REDIS_COMMAND_{command_callback_name_uppercase}_CALLBACK_PROVIDED",
            "MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_END({command_callback_name}) {{",
            "    char error_message[] = \"Command not implemented!\";",
            "    module_redis_connection_error_message_printf_noncritical(connection_context, error_message);",
            "    return true;",
            "}}",
            "#endif",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_callback_name_uppercase=command_info["command_callback_name"].upper(),
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info),
            tag_name=self._generate_commands_module_redis_command_autogenerated_tag_name(
                command_info=command_info)) + "\n"

    def _generate_commands_module_redis_command_autogenerated_c_command_free(
            self,
            command_info: dict) -> str:
        return "\n".join([
            "MODULE_REDIS_COMMAND_FUNCPTR_COMMAND_FREE_AUTOGEN({command_callback_name}) {{",
            "    {command_context_struct_name}_free(connection_context->db, connection_context->command.context);",
            "    return true;",
            "}}",
        ]).format(
            command_callback_name=command_info["command_callback_name"],
            command_context_struct_name=self._generate_command_context_struct_name(command_info),
            command_context_typedef_name=self._generate_command_context_typedef_name(command_info)) + "\n"

    def _generate_commands_module_redis_autogenerated_commands_callbacks_c(
            self,
            fp,
            command_info: dict):
        # TODO: when support for special types will be added it will be necessary to check if extra headers will
        #       be needed, e.g. for the hashtables, sets, lists extra needed will certainly have to be added

        fp.writelines([
            self._generate_commands_module_redis_command_autogenerated_c_headers(
                command_info=command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_command_end(
                command_info=command_info), "\n",
            self._generate_commands_module_redis_command_autogenerated_c_command_free(
                command_info=command_info), "\n",
        ])

    def _generate_commands_module_redis_autogenerated_commands_contexts_h_header(
            self,
            commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path,
                "module_redis_autogenerated_commands_contexts.h"), "w") as fp:

            self._write_header_header(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMAND_CONTEXTS_H")

            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_header_file_command_context_struct(
                        command_info=command_info,
                        arguments=command_info["arguments"],
                        is_sub_argument=False,
                        command_context_struct_name_suffix=""), "\n",
                ])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMAND_CONTEXTS_H")

    def _generate_commands_module_redis_autogenerated_commands_callbacks_c_code(self, commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path,
                "module_redis_autogenerated_commands_callbacks.c"), "w") as fp_commands_callbacks:

            self._generate_commands_module_redis_command_autogenerated_c_headers_general(fp_commands_callbacks)

            for command_info in commands_info:
                self._generate_commands_module_redis_autogenerated_commands_callbacks_c(
                    fp=fp_commands_callbacks,
                    command_info=command_info)

    def _generate_commands_scaffolding(
            self,
            commands_info: list):
        self._generate_commands_module_redis_autogenerated_commands_enum_h_header(
            commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_arguments_h_header(
            commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_key_specs_h_header(
            commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_info_map_h_header(
            commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_contexts_h_header(
            commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_contexts_c_code(
            commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_callbacks_h_header(
            commands_info=commands_info)
        self._generate_commands_module_redis_autogenerated_commands_callbacks_c_code(
            commands_info=commands_info)

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
