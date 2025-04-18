// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cripc/ipc_message_utils.h"

#include <stddef.h>
#include <stdint.h>
#include <tchar.h>

#include <memory>

#include "crbase/files/file_path.h"
#include "crbase/values.h"
#include "crbase/json/json_writer.h"
#include "crbase/strings/nullable_string16.h"
#include "crbase/strings/string_number_conversions.h"
#include "crbase/strings/utf_string_conversions.h"
#include "crbase/time/time.h"
#include "crbase/values.h"
#include "crbase/memory/shared_memory_handle.h"

#include "cripc/ipc_channel_handle.h"
#include "cripc/ipc_message.h"
///#include "cripc/handle_win.h"

namespace cripc {

namespace {

const int kMaxRecursionDepth = 100;

template<typename CharType>
void LogBytes(const std::vector<CharType>& data, std::string* out) {
  // Windows has a GUI for logging, which can handle arbitrary binary data.
  for (size_t i = 0; i < data.size(); ++i)
    out->push_back(data[i]);
}

bool ReadValue(const Message* m,
               cr::PickleIterator* iter,
               cr::Value** value,
               int recursion);

void WriteValue(Message* m, const cr::Value* value, int recursion) {
  bool result;
  if (recursion > kMaxRecursionDepth) {
    CR_LOG(WARNING) << "Max recursion depth hit in WriteValue.";
    return;
  }

  m->WriteInt(value->GetType());

  switch (value->GetType()) {
    case cr::Value::TYPE_NULL:
    break;
    case cr::Value::TYPE_BOOLEAN: {
      bool val;
      result = value->GetAsBoolean(&val);
      CR_DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case cr::Value::TYPE_INTEGER: {
      int val;
      result = value->GetAsInteger(&val);
      CR_DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case cr::Value::TYPE_DOUBLE: {
      double val;
      result = value->GetAsDouble(&val);
      CR_DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case cr::Value::TYPE_STRING: {
      std::string val;
      result = value->GetAsString(&val);
      CR_DCHECK(result);
      WriteParam(m, val);
      break;
    }
    case cr::Value::TYPE_BINARY: {
      const cr::BinaryValue* binary =
          static_cast<const cr::BinaryValue*>(value);
      m->WriteData(binary->GetBuffer(), static_cast<int>(binary->GetSize()));
      break;
    }
    case cr::Value::TYPE_DICTIONARY: {
      const cr::DictionaryValue* dict =
          static_cast<const cr::DictionaryValue*>(value);

      WriteParam(m, static_cast<int>(dict->size()));

      for (cr::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
           it.Advance()) {
        WriteParam(m, it.key());
        WriteValue(m, &it.value(), recursion + 1);
      }
      break;
    }
    case cr::Value::TYPE_LIST: {
      const cr::ListValue* list =
          static_cast<const cr::ListValue*>(value);
      WriteParam(m, static_cast<int>(list->GetSize()));
      for (cr::ListValue::const_iterator it = list->begin();
           it != list->end(); ++it) {
        WriteValue(m, *it, recursion + 1);
      }
      break;
    }
  }
}

// Helper for ReadValue that reads a DictionaryValue into a pre-allocated
// object.
bool ReadDictionaryValue(const Message* m,
                         cr::PickleIterator* iter,
                         cr::DictionaryValue* value,
                         int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  for (int i = 0; i < size; ++i) {
    std::string key;
    cr::Value* subval;
    if (!ReadParam(m, iter, &key) ||
        !ReadValue(m, iter, &subval, recursion + 1))
      return false;
    value->SetWithoutPathExpansion(key, subval);
  }

  return true;
}

// Helper for ReadValue that reads a ReadListValue into a pre-allocated
// object.
bool ReadListValue(const Message* m,
                   cr::PickleIterator* iter,
                   cr::ListValue* value,
                   int recursion) {
  int size;
  if (!ReadParam(m, iter, &size))
    return false;

  for (int i = 0; i < size; ++i) {
    cr::Value* subval;
    if (!ReadValue(m, iter, &subval, recursion + 1))
      return false;
    value->Set(i, subval);
  }

  return true;
}

bool ReadValue(const Message* m,
               cr::PickleIterator* iter,
               cr::Value** value,
               int recursion) {
  if (recursion > kMaxRecursionDepth) {
    CR_LOG(WARNING) << "Max recursion depth hit in ReadValue.";
    return false;
  }

  int type;
  if (!ReadParam(m, iter, &type))
    return false;

  switch (type) {
    case cr::Value::TYPE_NULL:
      *value = cr::Value::CreateNullValue().release();
    break;
    case cr::Value::TYPE_BOOLEAN: {
      bool val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = new cr::FundamentalValue(val);
      break;
    }
    case cr::Value::TYPE_INTEGER: {
      int val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = new cr::FundamentalValue(val);
      break;
    }
    case cr::Value::TYPE_DOUBLE: {
      double val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = new cr::FundamentalValue(val);
      break;
    }
    case cr::Value::TYPE_STRING: {
      std::string val;
      if (!ReadParam(m, iter, &val))
        return false;
      *value = new cr::StringValue(val);
      break;
    }
    case cr::Value::TYPE_BINARY: {
      const char* data;
      size_t length;
      if (!iter->ReadData(&data, &length))
        return false;
      *value = cr::BinaryValue::CreateWithCopiedBuffer(data, length);
      break;
    }
    case cr::Value::TYPE_DICTIONARY: {
      std::unique_ptr<cr::DictionaryValue> val(
          new cr::DictionaryValue());
      if (!ReadDictionaryValue(m, iter, val.get(), recursion))
        return false;
      *value = val.release();
      break;
    }
    case cr::Value::TYPE_LIST: {
      std::unique_ptr<cr::ListValue> val(new cr::ListValue());
      if (!ReadListValue(m, iter, val.get(), recursion))
        return false;
      *value = val.release();
      break;
    }
    default:
    return false;
  }

  return true;
}

}  // namespace

// -----------------------------------------------------------------------------

LogData::LogData()
    : routing_id(0),
      type(0),
      sent(0),
      receive(0),
      dispatch(0) {
}

LogData::~LogData() {
}

void ParamTraits<bool>::Log(const param_type& p, std::string* l) {
  l->append(p ? "true" : "false");
}

void ParamTraits<signed char>::Log(const param_type& p, std::string* l) {
  l->append(cr::IntToString(p));
}

void ParamTraits<unsigned char>::Log(const param_type& p, std::string* l) {
  l->append(cr::UintToString(p));
}

void ParamTraits<short>::Log(const param_type& p, std::string* l) {
  l->append(cr::UintToString(p));
}

void ParamTraits<unsigned short>::Log(const param_type& p, std::string* l) {
  l->append(cr::UintToString(p));
}

void ParamTraits<int>::Log(const param_type& p, std::string* l) {
  l->append(cr::IntToString(p));
}

void ParamTraits<unsigned int>::Log(const param_type& p, std::string* l) {
  l->append(cr::UintToString(p));
}

void ParamTraits<long>::Log(const param_type& p, std::string* l) {
  l->append(cr::Int64ToString(static_cast<int64_t>(p)));
}

void ParamTraits<unsigned long>::Log(const param_type& p, std::string* l) {
  l->append(cr::Uint64ToString(static_cast<uint64_t>(p)));
}

void ParamTraits<long long>::Log(const param_type& p, std::string* l) {
  l->append(cr::Int64ToString(static_cast<int64_t>(p)));
}

void ParamTraits<unsigned long long>::Log(const param_type& p, std::string* l) {
  l->append(cr::Uint64ToString(p));
}

void ParamTraits<float>::Log(const param_type& p, std::string* l) {
  l->append(cr::StringPrintf("%e", p));
}

void ParamTraits<double>::Log(const param_type& p, std::string* l) {
  l->append(cr::StringPrintf("%e", p));
}


void ParamTraits<std::string>::Log(const param_type& p, std::string* l) {
  l->append(p);
}

void ParamTraits<cr::string16>::Log(const param_type& p, std::string* l) {
  l->append(cr::UTF16ToUTF8(p));
}

void ParamTraits<std::vector<char> >::Write(Message* m, const param_type& p) {
  if (p.empty()) {
    m->WriteData(NULL, 0);
  } else {
    m->WriteData(&p.front(), p.size());
  }
}

bool ParamTraits<std::vector<char>>::Read(const Message* m,
                                          cr::PickleIterator* iter,
                                          param_type* r) {
  const char *data;
  size_t data_size = 0;
  if (!iter->ReadData(&data, &data_size))
    return false;
  r->resize(data_size);
  if (data_size)
    memcpy(&r->front(), data, data_size);
  return true;
}

void ParamTraits<std::vector<char> >::Log(const param_type& p, std::string* l) {
  LogBytes(p, l);
}

void ParamTraits<std::vector<unsigned char> >::Write(Message* m,
                                                     const param_type& p) {
  if (p.empty()) {
    m->WriteData(NULL, 0);
  } else {
    m->WriteData(reinterpret_cast<const char*>(&p.front()), p.size());
  }
}

bool ParamTraits<std::vector<unsigned char>>::Read(const Message* m,
                                                   cr::PickleIterator* iter,
                                                   param_type* r) {
  const char *data;
  size_t data_size = 0;
  if (!iter->ReadData(&data, &data_size))
    return false;
  r->resize(data_size);
  if (data_size)
    memcpy(&r->front(), data, data_size);
  return true;
}

void ParamTraits<std::vector<unsigned char> >::Log(const param_type& p,
                                                   std::string* l) {
  LogBytes(p, l);
}

void ParamTraits<std::vector<bool> >::Write(Message* m, const param_type& p) {
  m->WriteLength(p.size());
  // Cast to bool below is required because libc++'s
  // vector<bool>::const_reference is different from bool, and we want to avoid
  // writing an extra specialization of ParamTraits for it.
  for (size_t i = 0; i < p.size(); i++)
    WriteParam(m, static_cast<bool>(p[i]));
}

bool ParamTraits<std::vector<bool>>::Read(const Message* m,
                                          cr::PickleIterator* iter,
                                          param_type* r) {
  size_t size;
  // ReadLength() checks for < 0 itself.
  if (!iter->ReadLength(&size))
    return false;
  r->resize(size);
  for (int i = 0; i < size; i++) {
    bool value;
    if (!ReadParam(m, iter, &value))
      return false;
    (*r)[i] = value;
  }
  return true;
}

void ParamTraits<std::vector<bool> >::Log(const param_type& p, std::string* l) {
  for (size_t i = 0; i < p.size(); ++i) {
    if (i != 0)
      l->push_back(' ');
    LogParam(static_cast<bool>(p[i]), l);
  }
}

void ParamTraits<cr::DictionaryValue>::Write(Message* m,
                                                 const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<cr::DictionaryValue>::Read(const Message* m,
                                                cr::PickleIterator* iter,
                                                param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) || type != cr::Value::TYPE_DICTIONARY)
    return false;

  return ReadDictionaryValue(m, iter, r, 0);
}

void ParamTraits<cr::DictionaryValue>::Log(const param_type& p,
                                               std::string* l) {
  std::string json;
  cr::JSONWriter::Write(p, &json);
  l->append(json);
}

///void ParamTraits<cr::SharedMemoryHandle>::Write(Message* m,
///                                                    const param_type& p) {
///  m->WriteBool(p.NeedsBrokering());
///
///  if (p.NeedsBrokering()) {
///    HandleWin handle_win(p.GetHandle(), HandleWin::DUPLICATE);
///    ParamTraits<HandleWin>::Write(m, handle_win);
///  } else {
///    m->WriteInt(HandleToLong(p.GetHandle()));
///  }
///}
///
///bool ParamTraits<cr::SharedMemoryHandle>::Read(const Message* m,
///                                                   cr::PickleIterator* iter,
///                                                   param_type* r) {
///  bool needs_brokering;
///  if (!iter->ReadBool(&needs_brokering))
///    return false;
///
///  if (needs_brokering) {
///    HandleWin handle_win;
///    if (!ParamTraits<HandleWin>::Read(m, iter, &handle_win))
///      return false;
///    *r = cr::SharedMemoryHandle(handle_win.get_handle(),
///                                    cr::GetCurrentProcId());
///    return true;
///  }
///
///  int handle_int;
///  if (!iter->ReadInt(&handle_int))
///    return false;
///  HANDLE handle = LongToHandle(handle_int);
///  *r = cr::SharedMemoryHandle(handle, cr::GetCurrentProcId());
///  return true;
///}
///
///void ParamTraits<cr::SharedMemoryHandle>::Log(const param_type& p,
///                                                  std::string* l) {
///  LogParam(p.GetHandle(), l);
///  l->append(" needs brokering: ");
///  LogParam(p.NeedsBrokering(), l);
///}

void ParamTraits<cr::FilePath>::Write(Message* m, const param_type& p) {
  p.WriteToPickle(m);
}

bool ParamTraits<cr::FilePath>::Read(const Message* m,
                                         cr::PickleIterator* iter,
                                         param_type* r) {
  return r->ReadFromPickle(iter);
}

void ParamTraits<cr::FilePath>::Log(const param_type& p, std::string* l) {
  ParamTraits<cr::FilePath::StringType>::Log(p.value(), l);
}

void ParamTraits<cr::ListValue>::Write(Message* m, const param_type& p) {
  WriteValue(m, &p, 0);
}

bool ParamTraits<cr::ListValue>::Read(const Message* m,
                                          cr::PickleIterator* iter,
                                          param_type* r) {
  int type;
  if (!ReadParam(m, iter, &type) || type != cr::Value::TYPE_LIST)
    return false;

  return ReadListValue(m, iter, r, 0);
}

void ParamTraits<cr::ListValue>::Log(const param_type& p, std::string* l) {
  std::string json;
  cr::JSONWriter::Write(p, &json);
  l->append(json);
}

void ParamTraits<cr::NullableString16>::Write(Message* m,
                                                  const param_type& p) {
  WriteParam(m, p.string());
  WriteParam(m, p.is_null());
}

bool ParamTraits<cr::NullableString16>::Read(const Message* m,
                                                  cr::PickleIterator* iter,
                                                  param_type* r) {
  cr::string16 string;
  if (!ReadParam(m, iter, &string))
    return false;
  bool is_null;
  if (!ReadParam(m, iter, &is_null))
    return false;
  *r = cr::NullableString16(string, is_null);
  return true;
}

void ParamTraits<cr::NullableString16>::Log(const param_type& p,
                                                std::string* l) {
  l->append("(");
  LogParam(p.string(), l);
  l->append(", ");
  LogParam(p.is_null(), l);
  l->append(")");
}

void ParamTraits<cr::File::Info>::Write(Message* m,
                                            const param_type& p) {
  WriteParam(m, p.size);
  WriteParam(m, p.is_directory);
  WriteParam(m, p.last_modified.ToDoubleT());
  WriteParam(m, p.last_accessed.ToDoubleT());
  WriteParam(m, p.creation_time.ToDoubleT());
}

bool ParamTraits<cr::File::Info>::Read(const Message* m,
                                           cr::PickleIterator* iter,
                                           param_type* p) {
  double last_modified, last_accessed, creation_time;
  if (!ReadParam(m, iter, &p->size) ||
      !ReadParam(m, iter, &p->is_directory) ||
      !ReadParam(m, iter, &last_modified) ||
      !ReadParam(m, iter, &last_accessed) ||
      !ReadParam(m, iter, &creation_time))
    return false;
  p->last_modified = cr::Time::FromDoubleT(last_modified);
  p->last_accessed = cr::Time::FromDoubleT(last_accessed);
  p->creation_time = cr::Time::FromDoubleT(creation_time);
  return true;
}

void ParamTraits<cr::File::Info>::Log(const param_type& p,
                                          std::string* l) {
  l->append("(");  
  LogParam(p.size, l);
  l->append(",");
  LogParam(p.is_directory, l);
  l->append(",");
  LogParam(p.last_modified.ToDoubleT(), l);
  l->append(",");
  LogParam(p.last_accessed.ToDoubleT(), l);
  l->append(",");
  LogParam(p.creation_time.ToDoubleT(), l);
  l->append(")");
}

void ParamTraits<cr::Time>::Write(Message* m, const param_type& p) {
  ParamTraits<int64_t>::Write(m, p.ToInternalValue());
}

bool ParamTraits<cr::Time>::Read(const Message* m,
                                     cr::PickleIterator* iter,
                                     param_type* r) {
  int64_t value;
  if (!ParamTraits<int64_t>::Read(m, iter, &value))
    return false;
  *r = cr::Time::FromInternalValue(value);
  return true;
}

void ParamTraits<cr::Time>::Log(const param_type& p, std::string* l) {
  ParamTraits<int64_t>::Log(p.ToInternalValue(), l);
}

void ParamTraits<cr::TimeDelta>::Write(Message* m, const param_type& p) {
  ParamTraits<int64_t>::Write(m, p.ToInternalValue());
}

bool ParamTraits<cr::TimeDelta>::Read(const Message* m,
                                          cr::PickleIterator* iter,
                                          param_type* r) {
  int64_t value;
  bool ret = ParamTraits<int64_t>::Read(m, iter, &value);
  if (ret)
    *r = cr::TimeDelta::FromInternalValue(value);

  return ret;
}

void ParamTraits<cr::TimeDelta>::Log(const param_type& p, std::string* l) {
  ParamTraits<int64_t>::Log(p.ToInternalValue(), l);
}

void ParamTraits<cr::TimeTicks>::Write(Message* m, const param_type& p) {
  ParamTraits<int64_t>::Write(m, p.ToInternalValue());
}

bool ParamTraits<cr::TimeTicks>::Read(const Message* m,
                                          cr::PickleIterator* iter,
                                          param_type* r) {
  int64_t value;
  bool ret = ParamTraits<int64_t>::Read(m, iter, &value);
  if (ret)
    *r = cr::TimeTicks::FromInternalValue(value);

  return ret;
}

void ParamTraits<cr::TimeTicks>::Log(const param_type& p, std::string* l) {
  ParamTraits<int64_t>::Log(p.ToInternalValue(), l);
}

void ParamTraits<ChannelHandle>::Write(Message* m, const param_type& p) {
  // On Windows marshalling pipe handle is not supported.
  CR_DCHECK(p.pipe.handle == NULL);
  WriteParam(m, p.name);
}

bool ParamTraits<ChannelHandle>::Read(const Message *m,
                                      cr::PickleIterator* iter,
                                      param_type *r) {
  return ReadParam(m, iter, &r->name);
}

void ParamTraits<ChannelHandle>::Log(const param_type& p,
                                     std::string* l) {
  l->append(cr::StringPrintf("ChannelHandle(%s)", p.name.c_str()));
}

void ParamTraits<LogData>::Write(Message* m, const param_type& p) {
  WriteParam(m, p.channel);
  WriteParam(m, p.routing_id);
  WriteParam(m, p.type);
  WriteParam(m, p.flags);
  WriteParam(m, p.sent);
  WriteParam(m, p.receive);
  WriteParam(m, p.dispatch);
  WriteParam(m, p.message_name);
  WriteParam(m, p.params);
}

bool ParamTraits<LogData>::Read(const Message* m,
                                cr::PickleIterator* iter,
                                param_type* r) {
  return
      ReadParam(m, iter, &r->channel) &&
      ReadParam(m, iter, &r->routing_id) &&
      ReadParam(m, iter, &r->type) &&
      ReadParam(m, iter, &r->flags) &&
      ReadParam(m, iter, &r->sent) &&
      ReadParam(m, iter, &r->receive) &&
      ReadParam(m, iter, &r->dispatch) &&
      ReadParam(m, iter, &r->message_name) &&
      ReadParam(m, iter, &r->params);
}

void ParamTraits<LogData>::Log(const param_type& p, std::string* l) {
  // Doesn't make sense to implement this!
}

void ParamTraits<Message>::Write(Message* m, const Message& p) {
  // Don't just write out the message. This is used to send messages between
  // NaCl (Posix environment) and the browser (could be on Windows). The message
  // header formats differ between these systems (so does handle sharing, but
  // we already asserted we don't have any handles). So just write out the
  // parts of the header we use.
  //
  // Be careful also to use only explicitly-sized types. The NaCl environment
  // could be 64-bit and the host browser could be 32-bits. The nested message
  // may or may not be safe to send between 32-bit and 64-bit systems, but we
  // leave that up to the code sending the message to ensure.
  m->WriteUInt32(static_cast<uint32_t>(p.routing_id()));
  m->WriteUInt32(p.type());
  m->WriteUInt32(p.flags());
  m->WriteData(p.payload(), static_cast<uint32_t>(p.payload_size()));
}

bool ParamTraits<Message>::Read(const Message* m,
                                cr::PickleIterator* iter,
                                Message* r) {
  uint32_t routing_id, type, flags;
  if (!iter->ReadUInt32(&routing_id) ||
      !iter->ReadUInt32(&type) ||
      !iter->ReadUInt32(&flags))
    return false;

  size_t payload_size;
  const char* payload;
  if (!iter->ReadData(&payload, &payload_size))
    return false;

  r->SetHeaderValues(static_cast<int32_t>(routing_id), type, flags);
  r->WriteBytes(payload, payload_size);
  return true;
}

void ParamTraits<Message>::Log(const Message& p, std::string* l) {
  l->append("<ipc::Message>");
}

// Note that HWNDs/HANDLE/HCURSOR/HACCEL etc are always 32 bits, even on 64
// bit systems. That's why we use the Windows macros to convert to 32 bits.
void ParamTraits<HANDLE>::Write(Message* m, const param_type& p) {
  m->WriteLong(HandleToLong(p));
}

bool ParamTraits<HANDLE>::Read(const Message* m,
                               cr::PickleIterator* iter,
                               param_type* r) {
  long temp;
  if (!iter->ReadLong(&temp))
    return false;
  *r = LongToHandle(temp);
  return true;
}

void ParamTraits<HANDLE>::Log(const param_type& p, std::string* l) {
  l->append(cr::StringPrintf("0x%p", p));
}

void ParamTraits<LOGFONT>::Write(Message* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(&p), sizeof(LOGFONT));
}

bool ParamTraits<LOGFONT>::Read(const Message* m,
                                cr::PickleIterator* iter,
                                param_type* r) {
  const char *data;
  size_t data_size = 0;
  if (iter->ReadData(&data, &data_size) && data_size == sizeof(LOGFONT)) {
    const LOGFONT *font = reinterpret_cast<LOGFONT*>(const_cast<char*>(data));
    if (_tcsnlen(font->lfFaceName, LF_FACESIZE) < LF_FACESIZE) {
      memcpy(r, data, sizeof(LOGFONT));
      return true;
    }
  }

  CR_NOTREACHED();
  return false;
}

void ParamTraits<LOGFONT>::Log(const param_type& p, std::string* l) {
  l->append(cr::StringPrintf("<LOGFONT>"));
}

void ParamTraits<MSG>::Write(Message* m, const param_type& p) {
  m->WriteData(reinterpret_cast<const char*>(&p), sizeof(MSG));
}

bool ParamTraits<MSG>::Read(const Message* m,
                            cr::PickleIterator* iter,
                            param_type* r) {
  const char *data;
  size_t data_size = 0;
  bool result = iter->ReadData(&data, &data_size);
  if (result && data_size == sizeof(MSG)) {
    memcpy(r, data, sizeof(MSG));
  } else {
    result = false;
    CR_NOTREACHED();
  }

  return result;
}

void ParamTraits<MSG>::Log(const param_type& p, std::string* l) {
  l->append("<MSG>");
}

}  // namespace cripc
