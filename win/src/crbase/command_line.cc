// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/command_line.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <ostream>

#include "crbase/files/file_path.h"
#include "crbase/logging.h"
#include "crbase/macros.h"
#include "crbase/strings/string_split.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/utf_string_conversions.h"

namespace crbase {

CommandLine* CommandLine::current_process_commandline_ = NULL;

namespace {

const CommandLine::CharType kSwitchTerminator[] =
    FILE_PATH_LITERAL("--");
const CommandLine::CharType kSwitchValueSeparator[] =
    FILE_PATH_LITERAL("=");

// Since we use a lazy match, make sure that longer versions (like "--") are
// listed before shorter versions (like "-") of similar prefixes.
// By putting slash last, we can control whether it is treaded as a switch
// value by changing the value of switch_prefix_count to be one less than
// the array size.
const CommandLine::CharType* const kSwitchPrefixes[] = {L"--", L"-", L"/"};
size_t switch_prefix_count = cr_arraysize(kSwitchPrefixes);

size_t GetSwitchPrefixLength(const CommandLine::StringType& string) {
  for (size_t i = 0; i < switch_prefix_count; ++i) {
    CommandLine::StringType prefix(kSwitchPrefixes[i]);
    if (string.compare(0, prefix.length(), prefix) == 0)
      return prefix.length();
  }
  return 0;
}

// Fills in |switch_string| and |switch_value| if |string| is a switch.
// This will preserve the input switch prefix in the output |switch_string|.
bool IsSwitch(const CommandLine::StringType& string,
              CommandLine::StringType* switch_string,
              CommandLine::StringType* switch_value) {
  switch_string->clear();
  switch_value->clear();
  size_t prefix_length = GetSwitchPrefixLength(string);
  if (prefix_length == 0 || prefix_length == string.length())
    return false;

  const size_t equals_position = string.find(kSwitchValueSeparator);
  *switch_string = string.substr(0, equals_position);
  if (equals_position != CommandLine::StringType::npos)
    *switch_value = string.substr(equals_position + 1);
  return true;
}

// Append switches and arguments, keeping switches before arguments.
void AppendSwitchesAndArguments(CommandLine* command_line,
                                const CommandLine::StringVector& argv) {
  bool parse_switches = true;
  for (size_t i = 1; i < argv.size(); ++i) {
    CommandLine::StringType arg = argv[i];
    TrimWhitespace(arg, TRIM_ALL, &arg);

    CommandLine::StringType switch_string;
    CommandLine::StringType switch_value;
    parse_switches &= (arg != kSwitchTerminator);
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      command_line->AppendSwitchNative(UTF16ToASCII(switch_string),
                                       switch_value);
    } else {
      command_line->AppendArgNative(arg);
    }
  }
}

// Quote a string as necessary for CommandLineToArgvW compatiblity *on Windows*.
string16 QuoteForCommandLineToArgvW(const string16& arg,
                                    bool quote_placeholders) {
  // We follow the quoting rules of CommandLineToArgvW.
  // http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
  string16 quotable_chars(L" \\\"");
  // We may also be required to quote '%', which is commonly used in a command
  // line as a placeholder. (It may be substituted for a string with spaces.)
  if (quote_placeholders)
    quotable_chars.push_back(L'%');
  if (arg.find_first_of(quotable_chars) == string16::npos) {
    // No quoting necessary.
    return arg;
  }

  string16 out;
  out.push_back(L'"');
  for (size_t i = 0; i < arg.size(); ++i) {
    if (arg[i] == '\\') {
      // Find the extent of this run of backslashes.
      size_t start = i, end = start + 1;
      for (; end < arg.size() && arg[end] == '\\'; ++end) {}
      size_t backslash_count = end - start;

      // Backslashes are escapes only if the run is followed by a double quote.
      // Since we also will end the string with a double quote, we escape for
      // either a double quote or the end of the string.
      if (end == arg.size() || arg[end] == '"') {
        // To quote, we need to output 2x as many backslashes.
        backslash_count *= 2;
      }
      for (size_t j = 0; j < backslash_count; ++j)
        out.push_back('\\');

      // Advance i to one before the end to balance i++ in loop.
      i = end - 1;
    } else if (arg[i] == '"') {
      out.push_back('\\');
      out.push_back('"');
    } else {
      out.push_back(arg[i]);
    }
  }
  out.push_back('"');

  return out;
}

}  // namespace

CommandLine::CommandLine(NoProgram no_program)
    : argv_(1),
      begin_args_(1) {
}

CommandLine::CommandLine(const FilePath& program)
    : argv_(1),
      begin_args_(1) {
  SetProgram(program);
}

CommandLine::CommandLine(int argc, const CommandLine::CharType* const* argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argc, argv);
}

CommandLine::CommandLine(const StringVector& argv)
    : argv_(1),
      begin_args_(1) {
  InitFromArgv(argv);
}

CommandLine::CommandLine(const CommandLine& other)
    : argv_(other.argv_),
      switches_(other.switches_),
      begin_args_(other.begin_args_) {
  ResetStringPieces();
}

CommandLine& CommandLine::operator=(const CommandLine& other) {
  argv_ = other.argv_;
  switches_ = other.switches_;
  begin_args_ = other.begin_args_;
  ResetStringPieces();
  return *this;
}

CommandLine::~CommandLine() {
}

// static
void CommandLine::set_slash_is_not_a_switch() {
  // The last switch prefix should be slash, so adjust the size to skip it.
  CR_DCHECK_EQ(
      wcscmp(kSwitchPrefixes[cr_arraysize(kSwitchPrefixes) - 1], L"/"), 0);
  switch_prefix_count = cr_arraysize(kSwitchPrefixes) - 1;
}

// static
bool CommandLine::Init(int argc, const char* const* argv) {
  if (current_process_commandline_) {
    // If this is intentional, Reset() must be called first. If we are using
    // the shared build mode, we have to share a single object across multiple
    // shared libraries.
    return false;
  }

  current_process_commandline_ = new CommandLine(NO_PROGRAM);
  current_process_commandline_->ParseFromString(::GetCommandLineW());

  return true;
}

// static
void CommandLine::Reset() {
  CR_DCHECK(current_process_commandline_);
  delete current_process_commandline_;
  current_process_commandline_ = NULL;
}

// static
CommandLine* CommandLine::ForCurrentProcess() {
  CR_DCHECK(current_process_commandline_);
  return current_process_commandline_;
}

// static
bool CommandLine::InitializedForCurrentProcess() {
  return !!current_process_commandline_;
}

// static
CommandLine CommandLine::FromString(const string16& command_line) {
  CommandLine cmd(NO_PROGRAM);
  cmd.ParseFromString(command_line);
  return cmd;
}

void CommandLine::InitFromArgv(int argc,
                               const CommandLine::CharType* const* argv) {
  StringVector new_argv;
  for (int i = 0; i < argc; ++i)
    new_argv.push_back(argv[i]);
  InitFromArgv(new_argv);
}

void CommandLine::InitFromArgv(const StringVector& argv) {
  argv_ = StringVector(1);
  switches_.clear();
  switches_by_stringpiece_.clear();
  begin_args_ = 1;
  SetProgram(argv.empty() ? FilePath() : FilePath(argv[0]));
  AppendSwitchesAndArguments(this, argv);
}

FilePath CommandLine::GetProgram() const {
  return FilePath(argv_[0]);
}

void CommandLine::SetProgram(const FilePath& program) {
  TrimWhitespace(program.value(), TRIM_ALL, &argv_[0]);
}

bool CommandLine::HasSwitch(const StringPiece& switch_string) const {
  CR_DCHECK_EQ(ToLowerASCII(switch_string), switch_string);
  return switches_by_stringpiece_.find(switch_string) !=
         switches_by_stringpiece_.end();
}

bool CommandLine::HasSwitch(const char switch_constant[]) const {
  return HasSwitch(StringPiece(switch_constant));
}

std::string CommandLine::GetSwitchValueASCII(
    const StringPiece& switch_string) const {
  StringType value = GetSwitchValueNative(switch_string);
  if (!IsStringASCII(value)) {
    CR_DLOG(WARNING)
        << "Value of switch (" << switch_string << ") must be ASCII.";
    return std::string();
  }
  return UTF16ToASCII(value);
}

FilePath CommandLine::GetSwitchValuePath(
    const StringPiece& switch_string) const {
  return FilePath(GetSwitchValueNative(switch_string));
}

CommandLine::StringType CommandLine::GetSwitchValueNative(
    const StringPiece& switch_string) const {
  CR_DCHECK_EQ(ToLowerASCII(switch_string), switch_string);
  auto result = switches_by_stringpiece_.find(switch_string);
  return result == switches_by_stringpiece_.end() ? StringType()
                                                  : *(result->second);
}

void CommandLine::AppendSwitch(const std::string& switch_string) {
  AppendSwitchNative(switch_string, StringType());
}

void CommandLine::AppendSwitchPath(const std::string& switch_string,
                                   const FilePath& path) {
  AppendSwitchNative(switch_string, path.value());
}

void CommandLine::AppendSwitchNative(const std::string& switch_string,
                                     const CommandLine::StringType& value) {
  const std::string switch_key = ToLowerASCII(switch_string);
  StringType combined_switch_string(ASCIIToUTF16(switch_key));

  size_t prefix_length = GetSwitchPrefixLength(combined_switch_string);
  auto insertion =
      switches_.insert(make_pair(switch_key.substr(prefix_length), value));
  if (!insertion.second)
    insertion.first->second = value;
  switches_by_stringpiece_[insertion.first->first] = &(insertion.first->second);
  // Preserve existing switch prefixes in |argv_|; only append one if necessary.
  if (prefix_length == 0)
    combined_switch_string = kSwitchPrefixes[0] + combined_switch_string;
  if (!value.empty())
    combined_switch_string += kSwitchValueSeparator + value;
  // Append the switch and update the switches/arguments divider |begin_args_|.
  argv_.insert(argv_.begin() + begin_args_++, combined_switch_string);
}

void CommandLine::AppendSwitchASCII(const std::string& switch_string,
                                    const std::string& value_string) {
  AppendSwitchNative(switch_string, ASCIIToUTF16(value_string));
}

void CommandLine::CopySwitchesFrom(const CommandLine& source,
                                   const char* const switches[],
                                   size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (source.HasSwitch(switches[i]))
      AppendSwitchNative(switches[i], source.GetSwitchValueNative(switches[i]));
  }
}

CommandLine::StringVector CommandLine::GetArgs() const {
  // Gather all arguments after the last switch (may include kSwitchTerminator).
  StringVector args(argv_.begin() + begin_args_, argv_.end());
  // Erase only the first kSwitchTerminator (maybe "--" is a legitimate page?)
  StringVector::iterator switch_terminator =
      std::find(args.begin(), args.end(), kSwitchTerminator);
  if (switch_terminator != args.end())
    args.erase(switch_terminator);
  return args;
}

void CommandLine::AppendArg(const std::string& value) {
  CR_DCHECK(IsStringUTF8(value));
  AppendArgNative(UTF8ToWide(value));
}

void CommandLine::AppendArgPath(const FilePath& path) {
  AppendArgNative(path.value());
}

void CommandLine::AppendArgNative(const CommandLine::StringType& value) {
  argv_.push_back(value);
}

void CommandLine::AppendArg(const std::string& value1,
                            const std::string& value2) {
  AppendArg(value1);
  AppendArg(value2);
}

void CommandLine::AppendArgNative(const StringType& value1,
                                  const StringType& value2) {
  argv_.push_back(value1);
  argv_.push_back(value2);
}

void CommandLine::AppendArguments(const CommandLine& other,
                                  bool include_program) {
  if (include_program)
    SetProgram(other.GetProgram());
  AppendSwitchesAndArguments(this, other.argv());
}

void CommandLine::PrependWrapper(const CommandLine::StringType& wrapper) {
  if (wrapper.empty())
    return;
  // The wrapper may have embedded arguments (like "gdb --args"). In this case,
  // we don't pretend to do anything fancy, we just split on spaces.
  StringVector wrapper_argv = SplitString(
      wrapper, FilePath::StringType(1, ' '), TRIM_WHITESPACE,
      SPLIT_WANT_ALL);
  // Prepend the wrapper and update the switches/arguments |begin_args_|.
  argv_.insert(argv_.begin(), wrapper_argv.begin(), wrapper_argv.end());
  begin_args_ += wrapper_argv.size();
}

void CommandLine::ParseFromString(const string16& command_line) {
  string16 command_line_string;
  TrimWhitespace(command_line, TRIM_ALL, &command_line_string);
  if (command_line_string.empty())
    return;

  int num_args = 0;
  wchar_t** args = NULL;
  args = ::CommandLineToArgvW(command_line_string.c_str(), &num_args);

  CR_DPLOG_IF(FATAL, !args)
      << "CommandLineToArgvW failed on command line: "
      << UTF16ToUTF8(command_line);
  InitFromArgv(num_args, args);
  LocalFree(args);
}

CommandLine::StringType CommandLine::GetCommandLineStringInternal(
    bool quote_placeholders) const {
  StringType string(argv_[0]);
  string = QuoteForCommandLineToArgvW(string, quote_placeholders);
  StringType params(GetArgumentsStringInternal(quote_placeholders));
  if (!params.empty()) {
    string.append(StringType(FILE_PATH_LITERAL(" ")));
    string.append(params);
  }
  return string;
}

CommandLine::StringType CommandLine::GetArgumentsStringInternal(
    bool quote_placeholders) const {
  StringType params;
  // Append switches and arguments.
  bool parse_switches = true;
  for (size_t i = 1; i < argv_.size(); ++i) {
    StringType arg = argv_[i];
    StringType switch_string;
    StringType switch_value;
    parse_switches &= arg != kSwitchTerminator;
    if (i > 1)
      params.append(StringType(FILE_PATH_LITERAL(" ")));
    if (parse_switches && IsSwitch(arg, &switch_string, &switch_value)) {
      params.append(switch_string);
      if (!switch_value.empty()) {
        switch_value =
            QuoteForCommandLineToArgvW(switch_value, quote_placeholders);
        params.append(kSwitchValueSeparator + switch_value);
      }
    } else {
      arg = QuoteForCommandLineToArgvW(arg, quote_placeholders);
      params.append(arg);
    }
  }
  return params;
}

void CommandLine::ResetStringPieces() {
  switches_by_stringpiece_.clear();
  for (const auto& entry : switches_)
    switches_by_stringpiece_[entry.first] = &(entry.second);
}

}  // namespace crbase
