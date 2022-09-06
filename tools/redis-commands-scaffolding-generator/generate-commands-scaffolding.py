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
        elif argument["type"] == "long_string":
            field_type = "module_redis_long_string_t"
        elif argument["type"] == "short_string":
            field_type = "module_redis_short_string_t"
        elif argument["type"] == "integer":
            field_type = "int64_t"
        elif argument["type"] == "unixtime":
            field_type = "uint64_t"
        elif argument["type"] == "bool":
            field_type = "bool"
        elif argument["type"] == "double":
            field_type = "long double"
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

        if argument["token"] is not None:
            lines.append("bool has_token")

        if argument["has_multiple_occurrences"]:
            lines.append("{field_type} *list".format(field_type=field_type))
            lines.append("int count")
        elif argument["type"] != "bool":
            lines.append("{field_type} value".format(field_type=field_type))

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

        header += (
            "void "
            "{command_context_struct_name}{command_context_struct_name_suffix}_free(\n"
            "        storage_db_t *db,\n"
            "        {command_context_struct_name}{command_context_struct_name_suffix}_t *context);\n").format(
            command_context_struct_name=command_context_struct_name,
            command_context_struct_name_suffix=command_context_struct_name_suffix)

        return header

    def _generate_commands_scaffolding_header_file_argument_structs_content(
            self,
            command_info: dict,
            arguments: list,
            command_context_struct_name: str,
            argument_decl_name: str,
            parent_argument_index: Optional[int],
            parent_argument_decl_name: str) -> str:
        argument_context_member_size_per_type = {
            "KEY": "sizeof(module_redis_key_t)",
            "LONG_STRING": "sizeof(module_redis_long_string_t)",
            "SHORT_STRING": "sizeof(module_redis_short_string_t)",
            "PATTERN": "sizeof(module_redis_pattern_t)",
            "BLOCK": "sizeof({command_context_struct_name}_subargument_{argument_name}_t)",
            "ONEOF": "sizeof({command_context_struct_name}_subargument_{argument_name}_t)",
        }

        return ",\n".join([
                "    {{\n        " +
                ",\n        ".join([
                    ".name = \"{name}\"",
                    ".type = MODULE_REDIS_COMMAND_ARGUMENT_TYPE_{type}",
                    ".since = \"{since}\"",
                    # parent can be both NULL or a pointer to an argument
                    ".parent_argument = {parent_argument}",
                    # token can be both NULL or a string, the quoting is taking care in the format below
                    ".token = {token}",
                    # sub_arguments can be both NULL or a pointer to an argument list
                    ".sub_arguments = {sub_arguments}",
                    ".sub_arguments_count = {sub_arguments_count}",
                    ".is_positional = {is_positional}",
                    ".is_optional = {is_optional}",
                    ".is_sub_argument = {is_sub_argument}",
                    ".has_sub_arguments = {has_sub_arguments}",
                    ".has_multiple_occurrences = {has_multiple_occurrences}",
                    ".has_multiple_token = {has_multiple_token}",
                    ".argument_context_member_size = {argument_context_member_size}",
                    ".argument_context_member_offset = "
                    "offsetof({command_context_struct_name}_t,{command_context_field_name})",
                ]).format(
                    name=argument["name"].replace("-", "_").lower(),
                    type=argument["type"].replace("-", "").upper(),
                    since=argument["since"],
                    parent_argument=(
                        "&{}_arguments[{}]".format(parent_argument_decl_name, parent_argument_index)
                        if parent_argument_index is not None else "NULL"),
                    token=(
                        "\"" + argument["token"] + "\""
                        if not argument["token"] is None else "NULL"),
                    sub_arguments=(
                        "{argument_decl_name}_subargument_{argument_name}_arguments".format(
                            argument_decl_name=argument_decl_name,
                            argument_name=argument["name"].replace("-", "_"))
                        if argument["has_sub_arguments"] else "NULL"),
                    argument_context_member_size=(
                        argument_context_member_size_per_type[argument["type"].replace("-", "").upper()].format(
                            command_context_struct_name=command_context_struct_name,
                            argument_name=argument["name"].replace("-", "_"))
                        if argument["type"].replace("-", "").upper() in argument_context_member_size_per_type else 0),
                    sub_arguments_count=len(argument["sub_arguments"]),
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
        argument_decl_name = "module_redis_command_{command_callback_name}{argument_decl_name_suffix}".format(
            command_callback_name=command_info["command_callback_name"],
            argument_decl_name_suffix=argument_decl_name_suffix)

        for argument_index, argument in enumerate(arguments):
            if len(argument["sub_arguments"]) == 0:
                continue

            header += self._generate_commands_scaffolding_header_file_command_arguments_structs_definitions(
                command_info=command_info,
                arguments=argument["sub_arguments"],
                argument_decl_name_suffix="{argument_decl_name_suffix}_subargument_{argument_name}".format(
                    argument_decl_name_suffix=argument_decl_name_suffix,
                    argument_name=argument["name"].replace("-", "_")),
                command_context_struct_name="{command_context_struct_name}_subargument_{argument_name}".format(
                    command_context_struct_name=command_context_struct_name,
                    argument_name=argument["name"].replace("-", "_")))

        header += "module_redis_command_argument_t {argument_decl_name}_arguments[];\n".format(
            argument_decl_name=argument_decl_name)

        return header

    def _generate_commands_scaffolding_header_file_command_arguments_structs_table(
            self,
            command_info: dict,
            arguments: list,
            argument_decl_name_suffix: str,
            command_context_struct_name: str,
            parent_argument_index: Optional[int],
            parent_argument_decl_name: str) -> str:
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
                parent_argument_index=argument_index,
                parent_argument_decl_name=argument_decl_name,
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
                    parent_argument_index=parent_argument_index,
                    parent_argument_decl_name=parent_argument_decl_name,
                    command_context_struct_name=command_context_struct_name,
                    argument_decl_name=argument_decl_name) + "\n" +
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
                    self._generate_commands_scaffolding_header_file_command_arguments_structs_definitions(
                        command_info=command_info,
                        arguments=command_info["arguments"],
                        argument_decl_name_suffix="",
                        command_context_struct_name=self._generate_command_context_struct_name(command_info)),
                    "\n",
                ])

            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_scaffolding_header_file_command_arguments_structs_table(
                        command_info=command_info,
                        arguments=command_info["arguments"],
                        argument_decl_name_suffix="",
                        command_context_struct_name=self._generate_command_context_struct_name(command_info),
                        parent_argument_index=None,
                        parent_argument_decl_name=""),
                    "\n",
                ])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_ARGUMENTS_H")

    def _generate_commands_module_redis_autogenerated_commands_info_map_h_header(
            self,
            commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path, "module_redis_autogenerated_commands_info_map.h"), "w") \
                as fp:

            self._write_header_header(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMANDS_INFO_MAP_H")

            fp.writelines(["module_redis_command_info_t command_infos_map[] = {\n"])
            for command_info in commands_info:
                fp.writelines([
                    "    MODULE_REDIS_COMMAND_AUTOGEN("
                    "{command_callback_name_uppercase}, "
                    "\"{command_string}\", "
                    "{command_callback_name}, "
                    "{required_arguments_count}, "
                    "{has_variable_arguments}, "
                    "{arguments_count}" 
                    "),".format(
                        command_callback_name_uppercase=command_info["command_callback_name"].upper(),
                        command_string=command_info["command_string"].lower(),
                        command_callback_name=command_info["command_callback_name"],
                        required_arguments_count=command_info["required_arguments_count"],
                        has_variable_arguments="true" if command_info["has_variable_arguments"] else "false",
                        arguments_count=len(command_info["arguments"])),
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
            "#include <stddef.h>",
            "#include <string.h>",
            "#include <strings.h>",
            "#include <arpa/inet.h>",
            "",
            "#include \"misc.h\"",
            "#include \"exttypes.h\"",
            "#include \"log/log.h\"",
            "#include \"clock.h\"",
            "#include \"spinlock.h\"",
            "#include \"transaction.h\"",
            "#include \"transaction_spinlock.h\"",
            "#include \"data_structures/small_circular_queue/small_circular_queue.h\"",
            "#include \"data_structures/double_linked_list/double_linked_list.h\"",
            "#include \"memory_allocator/fast_fixed_memory_allocator.h\"",
            "#include \"data_structures/hashtable/spsc/hashtable_spsc.h\"",
            "#include \"data_structures/queue_mpmc/queue_mpmc.h\"",
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
            "    module_redis_connection_error_message_printf_noncritical(connection_context, \"Command '{command_callback_name_uppercase}' not implemented!\");",
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
                        command_context_struct_name_suffix=""), "\n",
                ])

            self._write_header_footer(fp, "CACHEGRAND_MODULE_REDIS_AUTOGENERATED_COMMAND_CONTEXTS_H")

    def _generate_commands_module_redis_autogenerated_commands_contexts_c_code_free_context(
            self,
            command_info: dict,
            arguments: list,
            command_context_struct_name_suffix: str) -> str:
        code = ""

        command_context_struct_name = self._generate_command_context_struct_name(command_info)

        has_sub_arguments = False
        for argument_index, argument in enumerate(arguments):
            if len(argument["sub_arguments"]) == 0:
                continue
            has_sub_arguments = True

            code += self._generate_commands_module_redis_autogenerated_commands_contexts_c_code_free_context(
                command_info=command_info,
                arguments=argument["sub_arguments"],
                command_context_struct_name_suffix="{}_subargument_{}".format(
                    command_context_struct_name_suffix,
                    argument["name"].replace("-", "_")))

        if has_sub_arguments:
            code += "\n"

        code += (
            "void "
            "{command_context_struct_name}{command_context_struct_name_suffix}_free(\n"
            "        storage_db_t *db,\n"
            "        {command_context_struct_name}{command_context_struct_name_suffix}_t *context) {{\n").format(
            command_context_struct_name=command_context_struct_name,
            command_context_struct_name_suffix=command_context_struct_name_suffix)

        # If there are no arguments, everything needed is just the error_message and the has_error properties
        first_processed = False
        for argument_index, argument in enumerate(arguments):
            lines = []
            field_name = None
            free_memory_func = "fast_fixed_memory_allocator_mem_free"

            if first_processed:
                lines.append("")

            first_processed = True

            lines.append("// free {command_context_struct_name}{command_context_struct_name_suffix}.{argument_name}")

            # Identify the type of the field
            if argument["type"] in ["block", "oneof"]:
                free_memory_func = (
                    "{command_context_struct_name}"
                    "{command_context_struct_name_suffix}"
                    "_subargument_{argument_name}_free").format(
                    command_context_struct_name=command_context_struct_name,
                    command_context_struct_name_suffix=command_context_struct_name_suffix,
                    argument_name=argument["name"].replace("-", "_"))

                if argument["has_multiple_occurrences"]:
                    lines.append("if (context->{argument_name}.list) {{")
                    lines.append("    for(int i = 0; i < context->{argument_name}.count; i++) {{")
                    lines.append("        {free_memory_func}(db, &context->{argument_name}.list[i]);")
                    lines.append("    }}")
                    lines.append("    fast_fixed_memory_allocator_mem_free(context->{argument_name}.list);")
                    lines.append("}}")
                else:
                    lines.append("{free_memory_func}(db, &context->{argument_name}.value);")

            elif argument["type"] in ["long_string"]:
                free_memory_func = "storage_db_chunk_sequence_free"

                if argument["has_multiple_occurrences"]:
                    lines.append("if (context->{argument_name}.list) {{")
                    lines.append("    for(int i = 0; i < context->{argument_name}.count; i++) {{")
                    lines.append("        if (context->{argument_name}.list[i].chunk_sequence) {{")
                    lines.append("            storage_db_chunk_sequence_free(db, context->{argument_name}.list[i].chunk_sequence);")
                    lines.append("        }}")
                    lines.append("    }}")
                    lines.append("    fast_fixed_memory_allocator_mem_free(context->{argument_name}.list);")
                    lines.append("}}")
                else:
                    lines.append("if (context->{argument_name}.value.chunk_sequence) {{")
                    lines.append("    {free_memory_func}(db, context->{argument_name}.value.chunk_sequence);")
                    lines.append("}}")

            elif argument["type"] in ["key", "pattern", "short_string"]:
                if argument["type"] == "key":
                    field_name = "key"
                elif argument["type"] == "pattern":
                    field_name = "pattern"
                elif argument["type"] == "short_string":
                    field_name = "short_string"

                if argument["has_multiple_occurrences"]:
                    lines.append("if (context->{argument_name}.list) {{")
                    lines.append("    for(int i = 0; i < context->{argument_name}.count; i++) {{")
                    lines.append("        if (context->{argument_name}.list[i].{field_name}) {{")
                    lines.append("            fast_fixed_memory_allocator_mem_free(context->{argument_name}.list[i].{field_name});")
                    lines.append("        }}")
                    lines.append("    }}")
                    lines.append("    fast_fixed_memory_allocator_mem_free(context->{argument_name}.list);")
                    lines.append("}}")
                else:
                    lines.append("if (context->{argument_name}.value.{field_name}) {{")
                    lines.append("    fast_fixed_memory_allocator_mem_free(context->{argument_name}.value.{field_name});")
                    lines.append("}}")
            else:
                continue

            code += ("    " + "\n    ".join(lines)).format(
                command_context_struct_name=command_context_struct_name,
                command_context_struct_name_suffix=command_context_struct_name_suffix,
                free_memory_func=free_memory_func,
                field_name=field_name,
                argument_name=argument["name"].replace("-", "_")) + "\n"

        code += "}\n"

        return code

    def _generate_commands_module_redis_autogenerated_commands_contexts_c_code(
            self,
            commands_info: list):
        with open(path.join(
                self._arguments.commands_scaffolding_path,
                "module_redis_autogenerated_commands_contexts.c"), "w") as fp:
            self._generate_commands_module_redis_command_autogenerated_c_headers_general(fp)

            # TODO: when support for special types will be added it will be necessary to check if extra headers will
            #       be needed, e.g. for the hashtables, sets, lists extra needed will certainly have to be added
            for command_info in commands_info:
                fp.writelines([
                    self._generate_commands_module_redis_autogenerated_commands_contexts_c_code_free_context(
                        command_info=command_info,
                        arguments=command_info["arguments"],
                        command_context_struct_name_suffix=""), "\n",
                ])

    def _generate_commands_module_redis_autogenerated_commands_callbacks_c_code(
            self,
            commands_info: list):
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
