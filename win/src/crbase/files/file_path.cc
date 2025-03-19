// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/files/file_path.h"

#include <string.h>
#include <algorithm>

#include "crbase/macros.h"
#include "crbase/pickle.h"
#include "crbase/strings/string_piece.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/sys_string_conversions.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/logging.h"
#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include <windows.h>
#endif

namespace crbase {

using StringType = FilePath::StringType;
using StringPieceType = FilePath::StringPieceType;

namespace {

const char* const kCommonDoubleExtensionSuffixes[] = { "gz", "z", "bz2", "bz" };
const char* const kCommonDoubleExtensions[] = { "user.js" };

const FilePath::CharType kStringTerminator = FILE_PATH_LITERAL('\0');

// If this FilePath contains a drive letter specification, returns the
// position of the last character of the drive letter specification,
// otherwise returns npos.  This can only be true on Windows, when a pathname
// begins with a letter followed by a colon.  On other platforms, this always
// returns npos.
StringPieceType::size_type FindDriveLetter(StringPieceType path) {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  // This is dependent on an ASCII-based character set, but that's a
  // reasonable assumption.  iswalpha can be too inclusive here.
  if (path.length() >= 2 && path[1] == L':' &&
      ((path[0] >= L'A' && path[0] <= L'Z') ||
       (path[0] >= L'a' && path[0] <= L'z'))) {
    return 1;
  }
#endif  // FILE_PATH_USES_DRIVE_LETTERS
  return StringType::npos;
}

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
bool EqualDriveLetterCaseInsensitive(StringPieceType a, StringPieceType b) {
  size_t a_letter_pos = FindDriveLetter(a);
  size_t b_letter_pos = FindDriveLetter(b);

  if (a_letter_pos == StringType::npos || b_letter_pos == StringType::npos)
    return a == b;

  StringPieceType a_letter(a.substr(0, a_letter_pos + 1));
  StringPieceType b_letter(b.substr(0, b_letter_pos + 1));
  if (!StartsWith(a_letter, b_letter, CompareCase::INSENSITIVE_ASCII))
    return false;

  StringPieceType a_rest(a.substr(a_letter_pos + 1));
  StringPieceType b_rest(b.substr(b_letter_pos + 1));
  return a_rest == b_rest;
}
#endif  // defined(FILE_PATH_USES_DRIVE_LETTERS)

bool IsPathAbsolute(StringPieceType path) {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  StringType::size_type letter = FindDriveLetter(path);
  if (letter != StringType::npos) {
    // Look for a separator right after the drive specification.
    return path.length() > letter + 1 &&
        FilePath::IsSeparator(path[letter + 1]);
  }
  // Look for a pair of leading separators.
  return path.length() > 1 &&
      FilePath::IsSeparator(path[0]) && FilePath::IsSeparator(path[1]);
#else  // FILE_PATH_USES_DRIVE_LETTERS
  // Look for a separator in the first position.
  return path.length() > 0 && FilePath::IsSeparator(path[0]);
#endif  // FILE_PATH_USES_DRIVE_LETTERS
}

bool AreAllSeparators(const StringType& input) {
  for (StringType::const_iterator it = input.begin();
      it != input.end(); ++it) {
    if (!FilePath::IsSeparator(*it))
      return false;
  }

  return true;
}

// Find the position of the '.' that separates the extension from the rest
// of the file name. The position is relative to BaseName(), not value().
// Returns npos if it can't find an extension.
StringType::size_type FinalExtensionSeparatorPosition(const StringType& path) {
  // Special case "." and ".."
  if (path == FilePath::kCurrentDirectory || path == FilePath::kParentDirectory)
    return StringType::npos;

  return path.rfind(FilePath::kExtensionSeparator);
}

// Same as above, but allow a second extension component of up to 4
// characters when the rightmost extension component is a common double
// extension (gz, bz2, Z).  For example, foo.tar.gz or foo.tar.Z would have
// extension components of '.tar.gz' and '.tar.Z' respectively.
StringType::size_type ExtensionSeparatorPosition(const StringType& path) {
  const StringType::size_type last_dot = FinalExtensionSeparatorPosition(path);

  // No extension, or the extension is the whole filename.
  if (last_dot == StringType::npos || last_dot == 0U)
    return last_dot;

  const StringType::size_type penultimate_dot =
      path.rfind(FilePath::kExtensionSeparator, last_dot - 1);
  const StringType::size_type last_separator =
      path.find_last_of(FilePath::kSeparators, last_dot - 1,
                        FilePath::kSeparatorsLength - 1);

  if (penultimate_dot == StringType::npos ||
      (last_separator != StringType::npos &&
       penultimate_dot < last_separator)) {
    return last_dot;
  }

  for (size_t i = 0; i < cr_arraysize(kCommonDoubleExtensions); ++i) {
    StringType extension(path, penultimate_dot + 1);
    if (LowerCaseEqualsASCII(extension, kCommonDoubleExtensions[i]))
      return penultimate_dot;
  }

  StringType extension(path, last_dot + 1);
  for (size_t i = 0; i < cr_arraysize(kCommonDoubleExtensionSuffixes); ++i) {
    if (LowerCaseEqualsASCII(extension, kCommonDoubleExtensionSuffixes[i])) {
      if ((last_dot - penultimate_dot) <= 5U &&
          (last_dot - penultimate_dot) > 1U) {
        return penultimate_dot;
      }
    }
  }

  return last_dot;
}

// Returns true if path is "", ".", or "..".
bool IsEmptyOrSpecialCase(const StringType& path) {
  // Special cases "", ".", and ".."
  if (path.empty() || path == FilePath::kCurrentDirectory ||
      path == FilePath::kParentDirectory) {
    return true;
  }

  return false;
}

}  // namespace

FilePath::FilePath() {
}

FilePath::FilePath(const FilePath& that) : path_(that.path_) {
}

FilePath::FilePath(StringPieceType path) {
  path.CopyToString(&path_);
  StringType::size_type nul_pos = path_.find(kStringTerminator);
  if (nul_pos != StringType::npos)
    path_.erase(nul_pos, StringType::npos);
}

FilePath::~FilePath() {
}

FilePath& FilePath::operator=(const FilePath& that) {
  path_ = that.path_;
  return *this;
}

bool FilePath::operator==(const FilePath& that) const {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  return EqualDriveLetterCaseInsensitive(this->path_, that.path_);
#else  // defined(FILE_PATH_USES_DRIVE_LETTERS)
  return path_ == that.path_;
#endif  // defined(FILE_PATH_USES_DRIVE_LETTERS)
}

bool FilePath::operator!=(const FilePath& that) const {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  return !EqualDriveLetterCaseInsensitive(this->path_, that.path_);
#else  // defined(FILE_PATH_USES_DRIVE_LETTERS)
  return path_ != that.path_;
#endif  // defined(FILE_PATH_USES_DRIVE_LETTERS)
}

// static
bool FilePath::IsSeparator(CharType character) {
  for (size_t i = 0; i < kSeparatorsLength - 1; ++i) {
    if (character == kSeparators[i]) {
      return true;
    }
  }

  return false;
}

void FilePath::GetComponents(std::vector<StringType>* components) const {
  CR_DCHECK(components);
  if (!components)
    return;
  components->clear();
  if (value().empty())
    return;

  std::vector<StringType> ret_val;
  FilePath current = *this;
  FilePath base;

  // Capture path components.
  while (current != current.DirName()) {
    base = current.BaseName();
    if (!AreAllSeparators(base.value()))
      ret_val.push_back(base.value());
    current = current.DirName();
  }

  // Capture root, if any.
  base = current.BaseName();
  if (!base.value().empty() && base.value() != kCurrentDirectory)
    ret_val.push_back(current.BaseName().value());

  // Capture drive letter, if any.
  FilePath dir = current.DirName();
  StringType::size_type letter = FindDriveLetter(dir.value());
  if (letter != StringType::npos) {
    ret_val.push_back(StringType(dir.value(), 0, letter + 1));
  }

  *components = std::vector<StringType>(ret_val.rbegin(), ret_val.rend());
}

bool FilePath::IsParent(const FilePath& child) const {
  return AppendRelativePath(child, NULL);
}

bool FilePath::AppendRelativePath(const FilePath& child,
                                  FilePath* path) const {
  std::vector<StringType> parent_components;
  std::vector<StringType> child_components;
  GetComponents(&parent_components);
  child.GetComponents(&child_components);

  if (parent_components.empty() ||
      parent_components.size() >= child_components.size())
    return false;

  std::vector<StringType>::const_iterator parent_comp =
      parent_components.begin();
  std::vector<StringType>::const_iterator child_comp =
      child_components.begin();

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  // Windows can access case sensitive filesystems, so component
  // comparisions must be case sensitive, but drive letters are
  // never case sensitive.
  if ((FindDriveLetter(*parent_comp) != StringType::npos) &&
      (FindDriveLetter(*child_comp) != StringType::npos)) {
    if (!StartsWith(*parent_comp, *child_comp, CompareCase::INSENSITIVE_ASCII))
      return false;
    ++parent_comp;
    ++child_comp;
  }
#endif  // defined(FILE_PATH_USES_DRIVE_LETTERS)

  while (parent_comp != parent_components.end()) {
    if (*parent_comp != *child_comp)
      return false;
    ++parent_comp;
    ++child_comp;
  }

  if (path != NULL) {
    for (; child_comp != child_components.end(); ++child_comp) {
      *path = path->Append(*child_comp);
    }
  }
  return true;
}

// libgen's dirname and basename aren't guaranteed to be thread-safe and aren't
// guaranteed to not modify their input strings, and in fact are implemented
// differently in this regard on different platforms.  Don't use them, but
// adhere to their behavior.
FilePath FilePath::DirName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // The drive letter, if any, always needs to remain in the output.  If there
  // is no drive letter, as will always be the case on platforms which do not
  // support drive letters, letter will be npos, or -1, so the comparisons and
  // resizes below using letter will still be valid.
  StringType::size_type letter = FindDriveLetter(new_path.path_);

  StringType::size_type last_separator =
      new_path.path_.find_last_of(kSeparators, StringType::npos,
                                  kSeparatorsLength - 1);
  if (last_separator == StringType::npos) {
    // path_ is in the current directory.
    new_path.path_.resize(letter + 1);
  } else if (last_separator == letter + 1) {
    // path_ is in the root directory.
    new_path.path_.resize(letter + 2);
  } else if (last_separator == letter + 2 &&
             IsSeparator(new_path.path_[letter + 1])) {
    // path_ is in "//" (possibly with a drive letter); leave the double
    // separator intact indicating alternate root.
    new_path.path_.resize(letter + 3);
  } else if (last_separator != 0) {
    // path_ is somewhere else, trim the basename.
    new_path.path_.resize(last_separator);
  }

  new_path.StripTrailingSeparatorsInternal();
  if (!new_path.path_.length())
    new_path.path_ = kCurrentDirectory;

  return new_path;
}

FilePath FilePath::BaseName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // The drive letter, if any, is always stripped.
  StringType::size_type letter = FindDriveLetter(new_path.path_);
  if (letter != StringType::npos) {
    new_path.path_.erase(0, letter + 1);
  }

  // Keep everything after the final separator, but if the pathname is only
  // one character and it's a separator, leave it alone.
  StringType::size_type last_separator =
      new_path.path_.find_last_of(kSeparators, StringType::npos,
                                  kSeparatorsLength - 1);
  if (last_separator != StringType::npos &&
      last_separator < new_path.path_.length() - 1) {
    new_path.path_.erase(0, last_separator + 1);
  }

  return new_path;
}

StringType FilePath::Extension() const {
  FilePath base(BaseName());
  const StringType::size_type dot = ExtensionSeparatorPosition(base.path_);
  if (dot == StringType::npos)
    return StringType();

  return base.path_.substr(dot, StringType::npos);
}

StringType FilePath::FinalExtension() const {
  FilePath base(BaseName());
  const StringType::size_type dot = FinalExtensionSeparatorPosition(base.path_);
  if (dot == StringType::npos)
    return StringType();

  return base.path_.substr(dot, StringType::npos);
}

FilePath FilePath::RemoveExtension() const {
  if (Extension().empty())
    return *this;

  const StringType::size_type dot = ExtensionSeparatorPosition(path_);
  if (dot == StringType::npos)
    return *this;

  return FilePath(path_.substr(0, dot));
}

FilePath FilePath::RemoveFinalExtension() const {
  if (FinalExtension().empty())
    return *this;

  const StringType::size_type dot = FinalExtensionSeparatorPosition(path_);
  if (dot == StringType::npos)
    return *this;

  return FilePath(path_.substr(0, dot));
}

FilePath FilePath::InsertBeforeExtension(StringPieceType suffix) const {
  if (suffix.empty())
    return FilePath(path_);

  if (IsEmptyOrSpecialCase(BaseName().value()))
    return FilePath();

  StringType ext = Extension();
  StringType ret = RemoveExtension().value();
  suffix.AppendToString(&ret);
  ret.append(ext);
  return FilePath(ret);
}

FilePath FilePath::InsertBeforeExtensionASCII(StringPiece suffix)
    const {
  CR_DCHECK(IsStringASCII(suffix));
#if defined(MINI_CHROMIUM_OS_WIN)
  return InsertBeforeExtension(ASCIIToUTF16(suffix));
#elif defined(MINI_CHROMIUM_OS_POSIX)
  return InsertBeforeExtension(suffix);
#endif
}

FilePath FilePath::AddExtension(StringPieceType extension) const {
  if (IsEmptyOrSpecialCase(BaseName().value()))
    return FilePath();

  // If the new extension is "" or ".", then just return the current FilePath.
  if (extension.empty() ||
      (extension.size() == 1 && extension[0] == kExtensionSeparator))
    return *this;

  StringType str = path_;
  if (extension[0] != kExtensionSeparator &&
      *(str.end() - 1) != kExtensionSeparator) {
    str.append(1, kExtensionSeparator);
  }
  extension.AppendToString(&str);
  return FilePath(str);
}

FilePath FilePath::ReplaceExtension(StringPieceType extension) const {
  if (IsEmptyOrSpecialCase(BaseName().value()))
    return FilePath();

  FilePath no_ext = RemoveExtension();
  // If the new extension is "" or ".", then just remove the current extension.
  if (extension.empty() ||
      (extension.size() == 1 && extension[0] == kExtensionSeparator))
    return no_ext;

  StringType str = no_ext.value();
  if (extension[0] != kExtensionSeparator)
    str.append(1, kExtensionSeparator);
  extension.AppendToString(&str);
  return FilePath(str);
}

bool FilePath::MatchesExtension(StringPieceType extension) const {
  CR_DCHECK(extension.empty() || extension[0] == kExtensionSeparator);
  StringType current_extension = Extension();

  if (current_extension.length() != extension.length())
    return false;

  return FilePath::CompareEqualIgnoreCase(extension, current_extension);
}

FilePath FilePath::Append(StringPieceType component) const {
  StringPieceType appended = component;
  StringType without_nuls;

  StringType::size_type nul_pos = component.find(kStringTerminator);
  if (nul_pos != StringPieceType::npos) {
    component.substr(0, nul_pos).CopyToString(&without_nuls);
    appended = StringPieceType(without_nuls);
  }

  CR_DCHECK(!IsPathAbsolute(appended));

  if (path_.compare(kCurrentDirectory) == 0) {
    // Append normally doesn't do any normalization, but as a special case,
    // when appending to kCurrentDirectory, just return a new path for the
    // component argument.  Appending component to kCurrentDirectory would
    // serve no purpose other than needlessly lengthening the path, and
    // it's likely in practice to wind up with FilePath objects containing
    // only kCurrentDirectory when calling DirName on a single relative path
    // component.
    return FilePath(appended);
  }

  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // Don't append a separator if the path is empty (indicating the current
  // directory) or if the path component is empty (indicating nothing to
  // append).
  if (appended.length() > 0 && new_path.path_.length() > 0) {
    // Don't append a separator if the path still ends with a trailing
    // separator after stripping (indicating the root directory).
    if (!IsSeparator(new_path.path_[new_path.path_.length() - 1])) {
      // Don't append a separator if the path is just a drive letter.
      if (FindDriveLetter(new_path.path_) + 1 != new_path.path_.length()) {
        new_path.path_.append(1, kSeparators[0]);
      }
    }
  }

  appended.AppendToString(&new_path.path_);
  return new_path;
}

FilePath FilePath::Append(const FilePath& component) const {
  return Append(component.value());
}

FilePath FilePath::AppendASCII(StringPiece component) const {
  CR_DCHECK(crbase::IsStringASCII(component));
#if defined(MINI_CHROMIUM_OS_WIN)
  return Append(ASCIIToUTF16(component));
#elif defined(MINI_CHROMIUM_OS_POSIX)
  return Append(component);
#endif
}

bool FilePath::IsAbsolute() const {
  return IsPathAbsolute(path_);
}

bool FilePath::EndsWithSeparator() const {
  if (empty())
    return false;
  return IsSeparator(path_[path_.size() - 1]);
}

FilePath FilePath::AsEndingWithSeparator() const {
  if (EndsWithSeparator() || path_.empty())
    return *this;

  StringType path_str;
  path_str.reserve(path_.length() + 1);  // Only allocate string once.

  path_str = path_;
  path_str.append(&kSeparators[0], 1);
  return FilePath(path_str);
}

FilePath FilePath::StripTrailingSeparators() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  return new_path;
}

bool FilePath::ReferencesParent() const {
  std::vector<StringType> components;
  GetComponents(&components);

  std::vector<StringType>::const_iterator it = components.begin();
  for (; it != components.end(); ++it) {
    const StringType& component = *it;
    // Windows has odd, undocumented behavior with path components containing
    // only whitespace and . characters. So, if all we see is . and
    // whitespace, then we treat any .. sequence as referencing parent.
    // For simplicity we enforce this on all platforms.
    if (component.find_first_not_of(FILE_PATH_LITERAL(". \n\r\t")) ==
            std::string::npos &&
        component.find(kParentDirectory) != std::string::npos) {
      return true;
    }
  }
  return false;
}


#if defined(MINI_CHROMIUM_OS_POSIX)
// See file_path.h for a discussion of the encoding of paths on POSIX
// platforms.  These encoding conversion functions are not quite correct.

string16 FilePath::LossyDisplayName() const {
  return WideToUTF16(SysNativeMBToWide(path_));
}

std::string FilePath::MaybeAsASCII() const {
  if (crbase::IsStringASCII(path_))
    return path_;
  return std::string();
}

std::string FilePath::AsUTF8Unsafe() const {
#if defined(SYSTEM_NATIVE_UTF8)
  return value();
#else
  return WideToUTF8(SysNativeMBToWide(value()));
#endif
}

string16 FilePath::AsUTF16Unsafe() const {
#if defined(SYSTEM_NATIVE_UTF8)
  return UTF8ToUTF16(value());
#else
  return WideToUTF16(SysNativeMBToWide(value()));
#endif
}

// static
FilePath FilePath::FromUTF8Unsafe(const std::string& utf8) {
#if defined(SYSTEM_NATIVE_UTF8)
  return FilePath(utf8);
#else
  return FilePath(SysWideToNativeMB(UTF8ToWide(utf8)));
#endif
}

// static
FilePath FilePath::FromUTF16Unsafe(const string16& utf16) {
#if defined(SYSTEM_NATIVE_UTF8)
  return FilePath(UTF16ToUTF8(utf16));
#else
  return FilePath(SysWideToNativeMB(UTF16ToWide(utf16)));
#endif
}

#elif defined(MINI_CHROMIUM_OS_WIN)

string16 FilePath::LossyDisplayName() const {
  return path_;
}

std::string FilePath::MaybeAsASCII() const {
  if (crbase::IsStringASCII(path_))
    return UTF16ToASCII(path_);
  return std::string();
}

std::string FilePath::AsUTF8Unsafe() const {
  return WideToUTF8(value());
}

string16 FilePath::AsUTF16Unsafe() const {
  return value();
}

// static
FilePath FilePath::FromUTF8Unsafe(const std::string& utf8) {
  return FilePath(UTF8ToWide(utf8));
}

// static
FilePath FilePath::FromUTF16Unsafe(const string16& utf16) {
  return FilePath(utf16);
}

#endif  // MINI_CHROMIUM_OS_WIN 

void FilePath::WriteToPickle(Pickle* pickle) const {
#if defined(MINI_CHROMIUM_OS_WIN)
  pickle->WriteString16(path_);
#else
  pickle->WriteString(path_);
#endif
}

bool FilePath::ReadFromPickle(PickleIterator* iter) {
#if defined(MINI_CHROMIUM_OS_WIN)
  if (!iter->ReadString16(&path_))
    return false;
#else
  if (!iter->ReadString(&path_))
    return false;
#endif

  if (path_.find(kStringTerminator) != StringType::npos)
    return false;

  return true;
}

#if defined(MINI_CHROMIUM_OS_WIN)
// Windows specific implementation of file string comparisons.

int FilePath::CompareIgnoreCase(StringPieceType string1,
                                StringPieceType string2) {
  // Perform character-wise upper case comparison rather than using the
  // fully Unicode-aware CompareString(). For details see:
  // http://blogs.msdn.com/michkap/archive/2005/10/17/481600.aspx
  StringPieceType::const_iterator i1 = string1.begin();
  StringPieceType::const_iterator i2 = string2.begin();
  StringPieceType::const_iterator string1end = string1.end();
  StringPieceType::const_iterator string2end = string2.end();
  for ( ; i1 != string1end && i2 != string2end; ++i1, ++i2) {
    wchar_t c1 =
        (wchar_t)LOWORD(::CharUpperW((LPWSTR)(DWORD_PTR)MAKELONG(*i1, 0)));
    wchar_t c2 =
        (wchar_t)LOWORD(::CharUpperW((LPWSTR)(DWORD_PTR)MAKELONG(*i2, 0)));
    if (c1 < c2)
      return -1;
    if (c1 > c2)
      return 1;
  }
  if (i1 != string1end)
    return 1;
  if (i2 != string2end)
    return -1;
  return 0;
}

#else  //  << other (POSIX) >>

// Generic Posix system comparisons.
int FilePath::CompareIgnoreCase(StringPieceType string1,
                                StringPieceType string2) {
  // Specifically need null termianted strings for this API call.
  int comparison = strcasecmp(string1.as_string().c_str(),
    string2.as_string().c_str());
  if (comparison < 0)
    return -1;
  if (comparison > 0)
    return 1;
  return 0;
}

#endif  // OS versions of CompareIgnoreCase()

void FilePath::StripTrailingSeparatorsInternal() {
  // If there is no drive letter, start will be 1, which will prevent stripping
  // the leading separator if there is only one separator.  If there is a drive
  // letter, start will be set appropriately to prevent stripping the first
  // separator following the drive letter, if a separator immediately follows
  // the drive letter.
  StringType::size_type start = FindDriveLetter(path_) + 2;

  StringType::size_type last_stripped = StringType::npos;
  for (StringType::size_type pos = path_.length();
       pos > start && IsSeparator(path_[pos - 1]);
       --pos) {
    // If the string only has two separators and they're at the beginning,
    // don't strip them, unless the string began with more than two separators.
    if (pos != start + 1 || last_stripped == start + 2 ||
        !IsSeparator(path_[start - 1])) {
      path_.resize(pos - 1);
      last_stripped = pos;
    }
  }
}

FilePath FilePath::NormalizePathSeparators() const {
  return NormalizePathSeparatorsTo(kSeparators[0]);
}

FilePath FilePath::NormalizePathSeparatorsTo(CharType separator) const {
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
  StringType copy = path_;
  for (size_t i = 0; i < kSeparatorsLength; ++i) {
    std::replace(copy.begin(), copy.end(), kSeparators[i], separator);
  }
  return FilePath(copy);
#else
  return *this;
#endif
}

}  // namespace crbase

namespace std {

std::ostream& operator<<(std::ostream& out, const crbase::FilePath& value) {
  std::string str = crbase::UTF16ToUTF8(value.value());
  return out << str;
}

}  // namespace std
