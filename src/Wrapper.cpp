#include "Wrapper.h"

#include <stdlib.h>

#include <vector>

#include <boost/preprocessor.hpp>
#include <boost/python/raw_function.hpp>
#include <boost/thread/tss.hpp>

#include <descrobject.h>
#include <datetime.h>

#include "V8Internal.h"

#include "Context.h"
#include "Utils.h"

#include <unistd.h>
#include <signal.h>

#define TERMINATE_EXECUTION_CHECK(returnValue) \
  if(v8::V8::IsExecutionTerminating()) { \
    ::PyErr_Clear(); \
    ::PyErr_SetString(PyExc_RuntimeError, "execution is terminating"); \
    return returnValue; \
  }

#define CHECK_V8_CONTEXT(isolate) \
  if (!isolate->InContext()) { \
    throw CJavascriptException("Javascript object out of context", PyExc_UnboundLocalError); \
  }

std::ostream& operator <<(std::ostream& os, const CJavascriptObject& obj)
{
  obj.Dump(os);

  return os;
}

void CWrapper::Expose(void)
{
  PyDateTime_IMPORT;

  py::class_<CJavascriptObject, boost::noncopyable>("JSObject", py::no_init)
    .def("__getattr__", &CJavascriptObject::GetAttr)
    .def("__setattr__", &CJavascriptObject::SetAttr)
    .def("__delattr__", &CJavascriptObject::DelAttr)

    .def("__hash__", &CJavascriptObject::GetIdentityHash)
    .def("clone", &CJavascriptObject::Clone, "Clone the object.")

  #if PY_MAJOR_VERSION < 3
    .add_property("__members__", &CJavascriptObject::GetAttrList)
  #else
    .def("__dir__", &CJavascriptObject::GetAttrList)
  #endif

    // Emulating dict object
    .def("keys", &CJavascriptObject::GetAttrList, "Get a list of an object's attributes.")

    .def("__getitem__", &CJavascriptObject::GetAttr)
    .def("__setitem__", &CJavascriptObject::SetAttr)
    .def("__delitem__", &CJavascriptObject::DelAttr)

    .def("__contains__", &CJavascriptObject::Contains)

    .def(int_(py::self))
    .def(float_(py::self))
    .def(str(py::self))

    .def("__nonzero__", &CJavascriptObject::operator bool)
    .def("__eq__", &CJavascriptObject::Equals)
    .def("__ne__", &CJavascriptObject::Unequals)

	  .def("create", &CJavascriptFunction::CreateWithArgs,
         (py::arg("constructor"),
          py::arg("arguments") = py::tuple(),
          py::arg("propertiesObject") = py::dict(),
          py::arg("isolate") = CIsolatePtr()),
         "Creates a new object with the specified prototype object and properties.")
	  .staticmethod("create")
    ;

  py::class_<CJavascriptNull, py::bases<CJavascriptObject>, boost::noncopyable>("JSNull", py::no_init)
    .def(py::init<CIsolatePtr>())
    .def(py::init<>())
    .def("__nonzero__", &CJavascriptNull::nonzero)
    .def("__str__", &CJavascriptNull::str)
    ;

  py::class_<CJavascriptUndefined, py::bases<CJavascriptObject>, boost::noncopyable>("JSUndefined", py::no_init)
    .def(py::init<CIsolatePtr>())
    .def(py::init<>())
    .def("__nonzero__", &CJavascriptUndefined::nonzero)
    .def("__str__", &CJavascriptUndefined::str)
    ;

  py::class_<CJavascriptArray, py::bases<CJavascriptObject>, boost::noncopyable>("JSArray", py::no_init)
    .def(py::init<CIsolatePtr, py::object>())
    .def(py::init<py::object>())

    .def("__len__", &CJavascriptArray::Length)

    .def("__getitem__", &CJavascriptArray::GetItem)
    .def("__setitem__", &CJavascriptArray::SetItem)
    .def("__delitem__", &CJavascriptArray::DelItem)

    .def("__iter__", py::range(&CJavascriptArray::begin, &CJavascriptArray::end))

    .def("__contains__", &CJavascriptArray::Contains)
    ;

  py::class_<CJavascriptFunction, py::bases<CJavascriptObject>, boost::noncopyable>("JSFunction", py::no_init)
    .def("__call__", py::raw_function(&CJavascriptFunction::CallWithArgs))

    .def("apply", &CJavascriptFunction::ApplyJavascript,
         (py::arg("self"),
          py::arg("args") = py::list(),
          py::arg("kwds") = py::dict()),
          "Performs a function call using the parameters.")
    .def("apply", &CJavascriptFunction::ApplyPython,
         (py::arg("self"),
          py::arg("args") = py::list(),
          py::arg("kwds") = py::dict()),
          "Performs a function call using the parameters.")
    .def("invoke", &CJavascriptFunction::Invoke,
          (py::arg("args") = py::list(),
           py::arg("kwds") = py::dict()),
           "Performs a binding method call using the parameters.")

    .def("setName", &CJavascriptFunction::SetName)

    .add_property("name", &CJavascriptFunction::GetName, &CJavascriptFunction::SetName, "The name of function")
    .add_property("owner", &CJavascriptFunction::GetOwner)

    .add_property("linenum", &CJavascriptFunction::GetLineNumber, "The line number of function in the script")
    .add_property("colnum", &CJavascriptFunction::GetColumnNumber, "The column number of function in the script")
    .add_property("resname", &CJavascriptFunction::GetResourceName, "The resource name of script")
    .add_property("inferredname", &CJavascriptFunction::GetInferredName, "Name inferred from variable or property assignment of this function")
    .add_property("lineoff", &CJavascriptFunction::GetLineOffset, "The line offset of function in the script")
    .add_property("coloff", &CJavascriptFunction::GetColumnOffset, "The column offset of function in the script")
    ;

  py::objects::class_value_wrapper<boost::shared_ptr<CJavascriptObject>,
    py::objects::make_ptr_instance<CJavascriptObject,
    py::objects::pointer_holder<boost::shared_ptr<CJavascriptObject>,CJavascriptObject> > >();
}

CJavascriptNull::CJavascriptNull(CIsolatePtr isolate) :
    CJavascriptObject(isolate->GetIsolate())
{
}

CJavascriptUndefined::CJavascriptUndefined(CIsolatePtr isolate) :
    CJavascriptObject(isolate->GetIsolate())
{
}

CJavascriptNull::CJavascriptNull() :
    CJavascriptObject(v8::Isolate::GetCurrent())
{
}

CJavascriptUndefined::CJavascriptUndefined() :
    CJavascriptObject(v8::Isolate::GetCurrent())
{
}

void CPythonObject::ThrowIf(v8::Isolate* isolate)
{
  CPythonGIL python_gil;

  assert(PyErr_OCCURRED());

  v8::HandleScope handle_scope(isolate);

  PyObject *exc, *val, *trb;

  ::PyErr_Fetch(&exc, &val, &trb);
  ::PyErr_NormalizeException(&exc, &val, &trb);

  py::object type(py::handle<>(py::allow_null(exc))),
             value(py::handle<>(py::allow_null(val)));

  if (trb) py::decref(trb);

  std::string msg;

  if (::PyObject_HasAttrString(value.ptr(), "args"))
  {
    py::object args = value.attr("args");

    if (PyTuple_Check(args.ptr()))
    {
      for (Py_ssize_t i=0; i<PyTuple_GET_SIZE(args.ptr()); i++)
      {
        py::extract<const std::string> extractor(args[i]);

        if (extractor.check()) msg += extractor();
      }
    }
  }
  else if (::PyObject_HasAttrString(value.ptr(), "message"))
  {
    py::extract<const std::string> extractor(value.attr("message"));

    if (extractor.check()) msg = extractor();
  }
  else if (val)
  {
    if (PyBytes_CheckExact(val))
    {
      msg = PyBytes_AS_STRING(val);
    }
    else if (PyTuple_CheckExact(val))
    {
      for (int i=0; i<PyTuple_GET_SIZE(val); i++)
      {
        PyObject *item = PyTuple_GET_ITEM(val, i);

        if (item && PyBytes_CheckExact(item))
        {
          msg = PyBytes_AS_STRING(item);
          break;
        }
      }
    }
  }

  v8::Handle<v8::Value> error;

  if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_IndexError))
  {
    error = v8::Exception::RangeError(v8::String::NewFromUtf8(isolate, msg.c_str(), v8::String::kNormalString, msg.size()));
  }
  else if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_AttributeError))
  {
    error = v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, msg.c_str(), v8::String::kNormalString, msg.size()));
  }
  else if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_SyntaxError))
  {
    error = v8::Exception::SyntaxError(v8::String::NewFromUtf8(isolate, msg.c_str(), v8::String::kNormalString, msg.size()));
  }
  else if (::PyErr_GivenExceptionMatches(type.ptr(), ::PyExc_TypeError))
  {
    error = v8::Exception::TypeError(v8::String::NewFromUtf8(isolate, msg.c_str(), v8::String::kNormalString, msg.size()));
  }
  else
  {
    error = v8::Exception::Error(v8::String::NewFromUtf8(isolate, msg.c_str(), v8::String::kNormalString, msg.size()));
  }

  if (error->IsObject())
  {
    // FIXME How to trace the lifecycle of exception? and when to delete those object in the hidden value?

  #ifdef SUPPORT_TRACE_EXCEPTION_LIFECYCLE
    error->ToObject()->SetHiddenValue(v8::String::NewFromUtf8(isolate, "exc_type"),
                                      v8::External::New(isolate, ObjectTracer::Trace(error, new py::object(type)).Object()));
    error->ToObject()->SetHiddenValue(v8::String::NewFromUtf8(isolate, "exc_value"),
                                      v8::External::New(isolate, ObjectTracer::Trace(error, new py::object(value)).Object()));
  #else
    error->ToObject()->SetHiddenValue(v8::String::NewFromUtf8(isolate, "exc_type"),
                                      v8::External::New(isolate, new py::object(type)));
    error->ToObject()->SetHiddenValue(v8::String::NewFromUtf8(isolate, "exc_value"),
                                      v8::External::New(isolate, new py::object(value)));
  #endif
  }

  isolate->ThrowException(error);
}

#define _TERMINATE_CALLBACK_EXECUTION_CHECK(returnValue) \
  if(v8::V8::IsExecutionTerminating()) { \
    ::PyErr_Clear(); \
    ::PyErr_SetString(PyExc_RuntimeError, "execution is terminating"); \
    info.GetReturnValue().Set(returnValue); \
    return; \
  }

#define TRY_HANDLE_EXCEPTION(value) _TERMINATE_CALLBACK_EXECUTION_CHECK(value) \
                                    BEGIN_HANDLE_PYTHON_EXCEPTION \
                                    {
#define END_HANDLE_EXCEPTION(value) } \
                                    END_HANDLE_PYTHON_EXCEPTION \
                                    info.GetReturnValue().Set(value);

#define CALLBACK_RETURN(value) do { info.GetReturnValue().Set(value); return; } while(0);


void CPythonObject::NamedGetter(v8::Local<v8::String> prop, const v8::PropertyCallbackInfo<v8::Value>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()))

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  v8::String::Utf8Value name(prop);

  if (PyGen_Check(obj.ptr())) CALLBACK_RETURN(v8::Undefined(info.GetIsolate()));
  
  if (name.length() && **name == '_')
  {
    // return None for every attribute starting with "_"
    CALLBACK_RETURN(v8::Handle<v8::Value>());
  }

  if (::PyMapping_Check(obj.ptr()) &&
      ::PyMapping_HasKeyString(obj.ptr(), *name))
  {
    py::object result(py::handle<>(::PyMapping_GetItemString(obj.ptr(), *name)));

    if (!result.is_none()) CALLBACK_RETURN(Wrap(result, info.GetIsolate()));
  }

  PyObject *value = ::PyObject_GetAttrString(obj.ptr(), *name);

  if (!value)
  {
    if (PyErr_OCCURRED())
    {
      if (::PyErr_ExceptionMatches(::PyExc_AttributeError))
      {
        ::PyErr_Clear();
      }
      else
      {
        py::throw_error_already_set();
      }
    }
    
    CALLBACK_RETURN(v8::Handle<v8::Value>());
  }

  py::object attr = py::object(py::handle<>(value));

#ifdef SUPPORT_PROPERTY
  if (PyObject_TypeCheck(attr.ptr(), &::PyProperty_Type))
  {
    py::object getter = attr.attr("fget");

    if (getter.is_none())
      throw CJavascriptException("unreadable attribute", ::PyExc_AttributeError);

    attr = getter();
  }
#endif

  CALLBACK_RETURN(Wrap(attr, info.GetIsolate()));

  END_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()))
}

void CPythonObject::NamedSetter(v8::Local<v8::String> prop, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()))

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  v8::String::Utf8Value name(prop);
  
  if (name.length() && **name == '_')
  {
    // return None for every attribute starting with "_"
    CALLBACK_RETURN(value);
  }
  
  py::object newval = CJavascriptObject::Wrap(value, info.GetIsolate());

  bool found = 1 == ::PyObject_HasAttrString(obj.ptr(), *name);

  if (::PyObject_HasAttrString(obj.ptr(), "__watchpoints__"))
  {
    py::dict watchpoints(obj.attr("__watchpoints__"));
    py::str propname(*name, name.length());

    if (watchpoints.has_key(propname))
    {
      py::object watchhandler = watchpoints.get(propname);

      newval = watchhandler(propname, found ? obj.attr(propname) : py::object(), newval);
    }
  }

  if (!found && ::PyMapping_Check(obj.ptr()))
  {
    ::PyMapping_SetItemString(obj.ptr(), *name, newval.ptr());
  }
  else
  {
  #ifdef SUPPORT_PROPERTY
    if (found)
    {
      py::object attr = obj.attr(*name);

      if (PyObject_TypeCheck(attr.ptr(), &::PyProperty_Type))
      {
        py::object setter = attr.attr("fset");

        if (setter.is_none())
          throw CJavascriptException("can't set attribute", ::PyExc_AttributeError);

        setter(newval);

        CALLBACK_RETURN(value);
      }
    }
  #endif
    obj.attr(*name) = newval;
  }

  CALLBACK_RETURN(value);

  END_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()));
}

void CPythonObject::NamedQuery(v8::Local<v8::String> prop, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Handle<v8::Integer>())

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  v8::String::Utf8Value name(prop);
  
  bool exists;
  
  if (name.length() && **name == '_')
  {
    // return None for every attribute starting with "_"
    exists = false;
  }
  else
  {
    exists = PyGen_Check(obj.ptr()) || ::PyObject_HasAttrString(obj.ptr(), *name) ||
             (::PyMapping_Check(obj.ptr()) && ::PyMapping_HasKeyString(obj.ptr(), *name));
  }

  if (exists) CALLBACK_RETURN(v8::Integer::New(info.GetIsolate(), v8::None));

  END_HANDLE_EXCEPTION(v8::Handle<v8::Integer>())
}

void CPythonObject::NamedDeleter(v8::Local<v8::String> prop, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Handle<v8::Boolean>())

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  v8::String::Utf8Value name(prop);
  if (name.length() && **name == '_')
  {
    CALLBACK_RETURN(false);
  }

  if (!::PyObject_HasAttrString(obj.ptr(), *name) &&
      ::PyMapping_Check(obj.ptr()) &&
      ::PyMapping_HasKeyString(obj.ptr(), *name))
  {
    CALLBACK_RETURN(-1 != ::PyMapping_DelItemString(obj.ptr(), *name));
  }
  else
  {
  #ifdef SUPPORT_PROPERTY
    py::object attr = obj.attr(*name);

    if (::PyObject_HasAttrString(obj.ptr(), *name) &&
        PyObject_TypeCheck(attr.ptr(), &::PyProperty_Type))
    {
      py::object deleter = attr.attr("fdel");

      if (deleter.is_none())
        throw CJavascriptException("can't delete attribute", ::PyExc_AttributeError);

      CALLBACK_RETURN(py::extract<bool>(deleter()));
    }
    else
    {
      CALLBACK_RETURN(-1 != ::PyObject_DelAttrString(obj.ptr(), *name));
    }
  #else
    CALLBACK_RETURN(-1 != ::PyObject_DelAttrString(obj.ptr(), *name));
  #endif
  }

  END_HANDLE_EXCEPTION(v8::Handle<v8::Boolean>())
}

#pragma GCC diagnostic ignored "-Wdeprecated-writable-strings"

void CPythonObject::NamedEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Handle<v8::Array>())

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  py::list keys;
  bool filter_name = false;

  if (::PySequence_Check(obj.ptr()))
  {
    CALLBACK_RETURN(v8::Handle<v8::Array>());
  }
  else if (::PyMapping_Check(obj.ptr()))
  {
    keys = py::list(py::handle<>(PyMapping_Keys(obj.ptr())));
  }
  else if (PyGen_CheckExact(obj.ptr()))
  {
    py::object iter(py::handle<>(::PyObject_GetIter(obj.ptr())));

    PyObject *item = NULL;

    while (NULL != (item = ::PyIter_Next(iter.ptr())))
    {
      keys.append(py::object(py::handle<>(item)));
    }
  }
  else
  {
    keys = py::list(py::handle<>(::PyObject_Dir(obj.ptr())));
    filter_name = true;
  }

  Py_ssize_t len = PyList_GET_SIZE(keys.ptr());
  v8::Handle<v8::Array> result = v8::Array::New(info.GetIsolate(), len);

  if (len > 0)
  {
    for (Py_ssize_t i=0; i<len; i++)
    {
      PyObject *item = PyList_GET_ITEM(keys.ptr(), i);

      if (filter_name && PyBytes_CheckExact(item))
      {
        py::str name(py::handle<>(py::borrowed(item)));

        // FIXME Are there any methods to avoid such a dirty work?

        if (name.startswith("_"))
          continue;
      }

      result->Set(v8::Uint32::New(info.GetIsolate(), i), Wrap(py::object(py::handle<>(py::borrowed(item))), info.GetIsolate()));
    }

    CALLBACK_RETURN(result);
  }

  END_HANDLE_EXCEPTION(v8::Handle<v8::Array>())
}

void CPythonObject::IndexedGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()));

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  if (PyGen_Check(obj.ptr())) CALLBACK_RETURN(v8::Undefined(info.GetIsolate()));

  if (::PySequence_Check(obj.ptr()))
  {
    if ((Py_ssize_t) index < ::PySequence_Size(obj.ptr()))
    {
      py::object ret(py::handle<>(::PySequence_GetItem(obj.ptr(), index)));

      CALLBACK_RETURN(Wrap(ret, info.GetIsolate()));
    }
  }
  else if (::PyMapping_Check(obj.ptr()))
  {
    char buf[65];

    snprintf(buf, sizeof(buf), "%d", index);

    PyObject *value = ::PyMapping_GetItemString(obj.ptr(), buf);

    if (!value)
    {
      py::long_ key(index);

      value = ::PyObject_GetItem(obj.ptr(), key.ptr());
    }

    if (value)
    {
      CALLBACK_RETURN(Wrap(py::object(py::handle<>(value)), info.GetIsolate()));
    }
  }

  END_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()))
}
void CPythonObject::IndexedSetter(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()));

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  if (::PySequence_Check(obj.ptr()))
  {
    if (::PySequence_SetItem(obj.ptr(), index, CJavascriptObject::Wrap(value, info.GetIsolate()).ptr()) < 0)
      info.GetIsolate()->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(info.GetIsolate(), "fail to set indexed value")));
  }
  else if (::PyMapping_Check(obj.ptr()))
  {
    char buf[65];

    snprintf(buf, sizeof(buf), "%d", index);

    if (::PyMapping_SetItemString(obj.ptr(), buf, CJavascriptObject::Wrap(value, info.GetIsolate()).ptr()) < 0)
      info.GetIsolate()->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(info.GetIsolate(), "fail to set named value")));
  }

  CALLBACK_RETURN(value);

  END_HANDLE_EXCEPTION(v8::Undefined(info.GetIsolate()))
}
void CPythonObject::IndexedQuery(uint32_t index, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Handle<v8::Integer>());

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  if (PyGen_Check(obj.ptr())) CALLBACK_RETURN(v8::Integer::New(info.GetIsolate(), v8::ReadOnly));

  if (::PySequence_Check(obj.ptr()))
  {
    if ((Py_ssize_t) index < ::PySequence_Size(obj.ptr()))
    {
      CALLBACK_RETURN(v8::Integer::New(info.GetIsolate(), v8::None));
    }
  }
  else if (::PyMapping_Check(obj.ptr()))
  {
    char buf[65];

    snprintf(buf, sizeof(buf), "%d", index);

    if (::PyMapping_HasKeyString(obj.ptr(), buf) ||
        ::PyMapping_HasKey(obj.ptr(), py::long_(index).ptr()))
    {
      CALLBACK_RETURN(v8::Integer::New(info.GetIsolate(), v8::None));
    }
  }

  END_HANDLE_EXCEPTION(v8::Handle<v8::Integer>())
}
void CPythonObject::IndexedDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Handle<v8::Boolean>());

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  if (::PySequence_Check(obj.ptr()) && (Py_ssize_t) index < ::PySequence_Size(obj.ptr()))
  {
    CALLBACK_RETURN(0 <= ::PySequence_DelItem(obj.ptr(), index));
  }
  else if (::PyMapping_Check(obj.ptr()))
  {
    char buf[65];

    snprintf(buf, sizeof(buf), "%d", index);

    CALLBACK_RETURN(PyMapping_DelItemString(obj.ptr(), buf) == 0);
  }

  END_HANDLE_EXCEPTION(v8::Handle<v8::Boolean>())
}

void CPythonObject::IndexedEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
{
  v8::HandleScope handle_scope(info.GetIsolate());

  TRY_HANDLE_EXCEPTION(v8::Handle<v8::Array>());

  CPythonGIL python_gil;

  py::object obj = CJavascriptObject::Wrap(info.Holder(), info.GetIsolate());

  Py_ssize_t len = ::PySequence_Check(obj.ptr()) ? ::PySequence_Size(obj.ptr()) : 0;

  v8::Handle<v8::Array> result = v8::Array::New(info.GetIsolate(), len);

  for (Py_ssize_t i=0; i<len; i++)
  {
    result->Set(v8::Uint32::New(info.GetIsolate(), i), v8::Int32::New(info.GetIsolate(), i)->ToString());
  }

  CALLBACK_RETURN(result);

  END_HANDLE_EXCEPTION(v8::Handle<v8::Array>())
}

#define GEN_ARG(z, n, data) CJavascriptObject::Wrap(info[n], info.GetIsolate())
#define GEN_ARGS(count) BOOST_PP_ENUM(count, GEN_ARG, NULL)

#define GEN_CASE_PRED(r, state) \
  BOOST_PP_NOT_EQUAL( \
    BOOST_PP_TUPLE_ELEM(2, 0, state), \
    BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2, 1, state)) \
  ) \
  /**/

#define GEN_CASE_OP(r, state) \
  ( \
    BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2, 0, state)), \
    BOOST_PP_TUPLE_ELEM(2, 1, state) \
  ) \
  /**/

#define GEN_CASE_MACRO(r, state) \
  case BOOST_PP_TUPLE_ELEM(2, 0, state): { \
    result = self(GEN_ARGS(BOOST_PP_TUPLE_ELEM(2, 0, state))); \
    break; \
  } \
  /**/

void CPythonObject::Caller(const v8::FunctionCallbackInfo<v8::Value>& info)
{
  v8::Isolate* isolate = info.GetIsolate();
  
  v8::HandleScope handle_scope(isolate);

  TRY_HANDLE_EXCEPTION(v8::Undefined(isolate));

  CPythonGIL python_gil;

  py::object self;

  if (!info.Data().IsEmpty() && info.Data()->IsExternal())
  {
    v8::Handle<v8::External> field = v8::Handle<v8::External>::Cast(info.Data());

    self = *static_cast<py::object *>(field->Value());
  }
  else
  {
    self = CJavascriptObject::Wrap(info.This(), isolate);
  }

  py::object result;

  switch (info.Length())
  {
    BOOST_PP_FOR((0, 10), GEN_CASE_PRED, GEN_CASE_OP, GEN_CASE_MACRO)
  default:
    info.GetIsolate()->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(info.GetIsolate(), "too many arguments")));

    CALLBACK_RETURN(v8::Undefined(isolate));
  }

  CALLBACK_RETURN(Wrap(result, isolate));

  END_HANDLE_EXCEPTION(v8::Undefined(isolate))
}

void CPythonObject::SetupObjectTemplate(v8::Isolate *isolate, v8::Handle<v8::ObjectTemplate> clazz)
{
  v8::HandleScope handle_scope(isolate);

  clazz->SetInternalFieldCount(1);
  clazz->SetNamedPropertyHandler(NamedGetter, NamedSetter, NamedQuery, NamedDeleter, NamedEnumerator);
  clazz->SetIndexedPropertyHandler(IndexedGetter, IndexedSetter, IndexedQuery, IndexedDeleter, IndexedEnumerator);
  clazz->SetCallAsFunctionHandler(Caller);
}

v8::Handle<v8::ObjectTemplate> CPythonObject::CreateObjectTemplate(v8::Isolate *isolate)
{
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::ObjectTemplate> clazz = v8::ObjectTemplate::New();

  SetupObjectTemplate(isolate, clazz);

  return handle_scope.Escape(clazz);
}

bool CPythonObject::IsWrapped(v8::Handle<v8::Object> obj)
{
  return obj->InternalFieldCount() == 1;
}

py::object CPythonObject::Unwrap(v8::Handle<v8::Object> obj, v8::Isolate* isolate)
{
  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::External> payload = v8::Handle<v8::External>::Cast(obj->GetInternalField(0));

  return *static_cast<py::object *>(payload->Value());
}

void CPythonObject::Dispose(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
  v8::HandleScope handle_scope(isolate);

  if (value->IsObject())
  {
    v8::Handle<v8::Object> obj = value->ToObject();

    if (IsWrapped(obj))
    {
      Py_DECREF(CPythonObject::Unwrap(obj, isolate).ptr());
    }
  }
}

v8::Handle<v8::Value> CPythonObject::Wrap(py::object obj, v8::Isolate* isolate)
{
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::Value> value;

#ifdef SUPPORT_TRACE_LIFECYCLE
  value = ObjectTracer::FindCache(obj);

  if (value.IsEmpty())
#endif

  value = WrapInternal(obj, isolate);

  return handle_scope.Escape(value);
}

v8::Handle<v8::Value> CPythonObject::WrapInternal(py::object obj, v8::Isolate* isolate)
{
  assert(isolate->InContext());

  v8::EscapableHandleScope handle_scope(isolate);

  v8::TryCatch try_catch;

  CPythonGIL python_gil;

  TERMINATE_EXECUTION_CHECK(v8::Undefined(isolate))

  if (obj.is_none()) return v8::Null(isolate);
  if (obj.ptr() == Py_True) return v8::True(isolate);
  if (obj.ptr() == Py_False) return v8::False(isolate);

  py::extract<CJavascriptObject&> extractor(obj);

  if (extractor.check())
  {
    CJavascriptObject& jsobj = extractor();

    if (dynamic_cast<CJavascriptNull *>(&jsobj)) return v8::Null(isolate);
    if (dynamic_cast<CJavascriptUndefined *>(&jsobj)) return v8::Undefined(isolate);

    if (jsobj.Object().IsEmpty())
    {
      ILazyObject *pLazyObject = dynamic_cast<ILazyObject *>(&jsobj);

      if (pLazyObject) pLazyObject->LazyConstructor();
    }

    if (jsobj.Object().IsEmpty())
    {
      throw CJavascriptException("Refer to a null object", ::PyExc_AttributeError);
    }

  #ifdef SUPPORT_TRACE_LIFECYCLE
    py::object *object = new py::object(obj);

    ObjectTracer::Trace(jsobj.Object(), object);
  #endif

    return handle_scope.Escape(jsobj.Object());
  }

  v8::Local<v8::Value> result;

#if PY_MAJOR_VERSION < 3
  if (PyInt_CheckExact(obj.ptr()))
  {
    result = v8::Integer::New(isolate, ::PyInt_AsLong(obj.ptr()));
  }
  else
#endif
  if (PyLong_CheckExact(obj.ptr()))
  {
    result = v8::Integer::New(isolate, ::PyLong_AsLong(obj.ptr()));
  }
  else if (PyBool_Check(obj.ptr()))
  {
    result = v8::Boolean::New(isolate, py::extract<bool>(obj));
  }
  else if (PyBytes_CheckExact(obj.ptr()) ||
           PyUnicode_CheckExact(obj.ptr()))
  {
    result = ToString(obj, isolate);
  }
  else if (PyFloat_CheckExact(obj.ptr()))
  {
    result = v8::Number::New(isolate, py::extract<double>(obj));
  }
  else if (PyDateTime_CheckExact(obj.ptr()) || PyDate_CheckExact(obj.ptr()))
  {
    tm ts = { 0 };

    ts.tm_year = PyDateTime_GET_YEAR(obj.ptr()) - 1900;
    ts.tm_mon = PyDateTime_GET_MONTH(obj.ptr()) - 1;
    ts.tm_mday = PyDateTime_GET_DAY(obj.ptr());
    ts.tm_hour = PyDateTime_DATE_GET_HOUR(obj.ptr());
    ts.tm_min = PyDateTime_DATE_GET_MINUTE(obj.ptr());
    ts.tm_sec = PyDateTime_DATE_GET_SECOND(obj.ptr());
    ts.tm_isdst = -1;

    int ms = PyDateTime_DATE_GET_MICROSECOND(obj.ptr());

    result = v8::Date::New(isolate, ((double) mktime(&ts)) * 1000 + ms / 1000);
  }
  else if (PyTime_CheckExact(obj.ptr()))
  {
    tm ts = { 0 };

    ts.tm_hour = PyDateTime_TIME_GET_HOUR(obj.ptr()) - 1;
    ts.tm_min = PyDateTime_TIME_GET_MINUTE(obj.ptr());
    ts.tm_sec = PyDateTime_TIME_GET_SECOND(obj.ptr());

    int ms = PyDateTime_TIME_GET_MICROSECOND(obj.ptr());

    result = v8::Date::New(isolate, ((double) mktime(&ts)) * 1000 + ms / 1000);
  }
  else if (PyCFunction_Check(obj.ptr()) || PyFunction_Check(obj.ptr()) ||
           PyMethod_Check(obj.ptr()) || PyType_Check(obj.ptr()))
  {
    v8::Handle<v8::FunctionTemplate> func_tmpl = v8::FunctionTemplate::New(isolate);
    py::object *object = new py::object(obj);

    func_tmpl->SetCallHandler(Caller, v8::External::New(isolate, object));

    if (PyType_Check(obj.ptr()))
    {
      v8::Handle<v8::String> cls_name = v8::String::NewFromUtf8(isolate, py::extract<const char *>(obj.attr("__name__"))());

      func_tmpl->SetClassName(cls_name);
    }

    result = func_tmpl->GetFunction();

  #ifdef SUPPORT_TRACE_LIFECYCLE
    if (!result.IsEmpty()) ObjectTracer::Trace(result, object);
  #endif
  }
  else
  {
    static boost::thread_specific_ptr< v8::Persistent<v8::ObjectTemplate> > s_template;
    
    if( !s_template.get() ) {
        s_template.reset( new v8::Persistent<v8::ObjectTemplate>(isolate, CreateObjectTemplate(isolate)) );
    }
    
    v8::Handle<v8::Object> instance = v8::Local<v8::ObjectTemplate>::New(isolate, *s_template.get())->NewInstance();

    if (!instance.IsEmpty())
    {
      py::object *object = new py::object(obj);

      instance->SetInternalField(0, v8::External::New(isolate, object));

    #ifdef SUPPORT_TRACE_LIFECYCLE
      ObjectTracer::Trace(instance, object);
    #endif
    }

    result = instance;
  }

  if (result.IsEmpty()) CJavascriptException::ThrowIf(isolate, try_catch);

  return handle_scope.Escape(result);
}

void CJavascriptObject::CheckAttr(v8::Handle<v8::String> name) const
{
  assert(m_isolate->InContext());

  v8::HandleScope handle_scope(m_isolate);

  if (!Object()->Has(name))
  {
    std::ostringstream msg;

    msg << "'" << *v8::String::Utf8Value(Object()->ObjectProtoToString())
        << "' object has no attribute '" << *v8::String::Utf8Value(name) << "'";

    throw CJavascriptException(msg.str(), ::PyExc_AttributeError);
  }
}

py::object CJavascriptObject::GetAttr(const std::string& name)
{
#ifdef SUPPORT_PROBES
  if (WRAPPER_JS_OBJECT_GETATTR_ENABLED()) {
    WRAPPER_JS_OBJECT_GETATTR(&m_obj, name.c_str());
  }
#endif

  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  v8::Handle<v8::String> attr_name = DecodeUtf8(name, m_isolate);

  CheckAttr(attr_name);

  v8::Handle<v8::Value> attr_value = Object()->Get(attr_name);

  if (attr_value.IsEmpty())
    CJavascriptException::ThrowIf(m_isolate, try_catch);

  return CJavascriptObject::Wrap(attr_value, m_isolate, Object());
}

void CJavascriptObject::SetAttr(const std::string& name, py::object value)
{
#ifdef SUPPORT_PROBES
  if (WRAPPER_JS_OBJECT_SETATTR_ENABLED()) {
    WRAPPER_JS_OBJECT_SETATTR(&m_obj, name.c_str(), value.ptr());
  }
#endif

  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  v8::Handle<v8::String> attr_name = DecodeUtf8(name, m_isolate);
  v8::Handle<v8::Value> attr_obj = CPythonObject::Wrap(value, m_isolate);

  if (Object()->Has(attr_name))
  {
    v8::Handle<v8::Value> UNUSED_VAR(attr_value) = Object()->Get(attr_name);
  }

  if (!Object()->Set(attr_name, attr_obj))
    CJavascriptException::ThrowIf(m_isolate, try_catch);
}
void CJavascriptObject::DelAttr(const std::string& name)
{
#ifdef SUPPORT_PROBES
  if (WRAPPER_JS_OBJECT_DELATTR_ENABLED()) {
    WRAPPER_JS_OBJECT_DELATTR(&m_obj, name.c_str());
  }
#endif

  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  v8::Handle<v8::String> attr_name = DecodeUtf8(name, m_isolate);

  CheckAttr(attr_name);

  if (!Object()->Delete(attr_name))
    CJavascriptException::ThrowIf(m_isolate, try_catch);
}
py::list CJavascriptObject::GetAttrList(void)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);
  CPythonGIL python_gil;

  py::list attrs;

  TERMINATE_EXECUTION_CHECK(attrs);

  v8::TryCatch try_catch;

  v8::Handle<v8::Array> props = Object()->GetPropertyNames();

  for (size_t i=0; i<props->Length(); i++)
  {
    attrs.append(CJavascriptObject::Wrap(props->Get(v8::Integer::New(m_isolate, i)), m_isolate));
  }

  if (try_catch.HasCaught()) CJavascriptException::ThrowIf(m_isolate, try_catch);

  return attrs;
}

int CJavascriptObject::GetIdentityHash(void)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  return Object()->GetIdentityHash();
}

CJavascriptObjectPtr CJavascriptObject::Clone(void)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  return CJavascriptObjectPtr(new CJavascriptObject(m_isolate, Object()->Clone()));
}

bool CJavascriptObject::Contains(const std::string& name)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  bool found = Object()->Has(DecodeUtf8(name, m_isolate));

  if (try_catch.HasCaught()) CJavascriptException::ThrowIf(m_isolate, try_catch);

  return found;
}

bool CJavascriptObject::Equals(CJavascriptObjectPtr other) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  return other.get() && Object()->Equals(other->Object());
}

void CJavascriptObject::Dump(std::ostream& os) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  if (m_obj.IsEmpty())
    os << "None";
  else if (Object()->IsInt32())
    os << Object()->Int32Value();
  else if (Object()->IsNumber())
    os << Object()->NumberValue();
  else if (Object()->IsBoolean())
    os << Object()->BooleanValue();
  else if (Object()->IsNull())
    os << "None";
  else if (Object()->IsUndefined())
    os << "N/A";
  else if (Object()->IsString())
    os << *v8::String::Utf8Value(v8::Handle<v8::String>::Cast(Object()));
  else
  {
    v8::Handle<v8::String> s = Object()->ToString();

    if (s.IsEmpty())
      s = Object()->ObjectProtoToString();

    if (!s.IsEmpty())
      os << *v8::String::Utf8Value(s);
  }
}

CJavascriptObject::operator long() const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  if (m_obj.IsEmpty())
    throw CJavascriptException("argument must be a string or a number, not 'NoneType'", ::PyExc_TypeError);

  return Object()->Int32Value();
}
CJavascriptObject::operator double() const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  if (m_obj.IsEmpty())
    throw CJavascriptException("argument must be a string or a number, not 'NoneType'", ::PyExc_TypeError);

  return Object()->NumberValue();
}

CJavascriptObject::operator bool() const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  if (m_obj.IsEmpty()) return false;

  return Object()->BooleanValue();
}

py::object CJavascriptObject::Wrap(v8::Handle<v8::Value> value, v8::Isolate* isolate, v8::Handle<v8::Object> self)
{
  assert(isolate->InContext());

  v8::HandleScope handle_scope(isolate);

  if (value.IsEmpty() || value->IsNull() || value->IsUndefined()) return py::object();
  if (value->IsTrue()) return py::object(py::handle<>(py::borrowed(Py_True)));
  if (value->IsFalse()) return py::object(py::handle<>(py::borrowed(Py_False)));

  if (value->IsInt32()) return py::object(value->Int32Value());
  if (value->IsString())
  {
    v8::String::Utf8Value str(v8::Handle<v8::String>::Cast(value));

    return py::str(*str, str.length());
  }
  if (value->IsStringObject())
  {
    v8::String::Utf8Value str(value.As<v8::StringObject>()->ValueOf());

    return py::str(*str, str.length());
  }
  if (value->IsBoolean())
  {
    return py::object(py::handle<>(py::borrowed(value->BooleanValue() ? Py_True : Py_False)));
  }
  if (value->IsBooleanObject())
  {
    return py::object(py::handle<>(py::borrowed(value.As<v8::BooleanObject>()->BooleanValue() ? Py_True : Py_False)));
  }
  if (value->IsNumber())
  {
    return py::object(py::handle<>(::PyFloat_FromDouble(value->NumberValue())));
  }
  if (value->IsNumberObject())
  {
    return py::object(py::handle<>(::PyFloat_FromDouble(value.As<v8::NumberObject>()->NumberValue())));
  }
  if (value->IsDate())
  {
    double n = v8::Handle<v8::Date>::Cast(value)->NumberValue();

    time_t ts = (time_t) floor(n / 1000);

    tm *t = localtime(&ts);

    return py::object(py::handle<>(::PyDateTime_FromDateAndTime(
      t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
      ((long long) floor(n)) % 1000 * 1000)));
  }

  return Wrap(value->ToObject(), isolate, self);
}

py::object CJavascriptObject::Wrap(v8::Handle<v8::Object> obj, v8::Isolate* isolate, v8::Handle<v8::Object> self)
{
  v8::HandleScope handle_scope(isolate);

  if (obj.IsEmpty())
  {
    return py::object();
  }
  else if (obj->IsArray())
  {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(obj);

    return Wrap(new CJavascriptArray(isolate, array), isolate);
  }
  else if (CPythonObject::IsWrapped(obj))
  {
    return CPythonObject::Unwrap(obj, isolate);
  }
  else if (obj->IsFunction())
  {
    return Wrap(new CJavascriptFunction(isolate, self, v8::Handle<v8::Function>::Cast(obj)), isolate);
  }

  return Wrap(new CJavascriptObject(isolate, obj), isolate);
}

py::object CJavascriptObject::Wrap(CJavascriptObject *obj, v8::Isolate* isolate)
{
  CPythonGIL python_gil;

  TERMINATE_EXECUTION_CHECK(py::object())

  return py::object(py::handle<>(boost::python::converter::shared_ptr_to_python<CJavascriptObject>(CJavascriptObjectPtr(obj))));
}

CJavascriptArray::CJavascriptArray(v8::Isolate* isolate, v8::Handle<v8::Array> array)
  : CJavascriptObject(isolate, array), m_size(array->Length())
{

}

CJavascriptArray::CJavascriptArray(CIsolatePtr isolate, py::object items)
  : CJavascriptObject(v8::Isolate::GetCurrent()), m_items(items), m_size(0)
{
}

CJavascriptArray::CJavascriptArray(py::object items)
  : CJavascriptObject(v8::Isolate::GetCurrent()), m_items(items), m_size(0)
{
}

void CJavascriptArray::LazyConstructor(void)
{
  if (!m_obj.IsEmpty()) return;

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Array> array;

  if (m_items.is_none())
  {
    array = v8::Array::New(m_isolate, m_size);
  }
#if PY_MAJOR_VERSION < 3
  else if (PyInt_CheckExact(m_items.ptr()))
  {
    m_size = PyInt_AS_LONG(m_items.ptr());
    array = v8::Array::New(m_isolate, m_size);
  }
#endif
  else if (PyLong_CheckExact(m_items.ptr()))
  {
    m_size = PyLong_AsLong(m_items.ptr());
    array = v8::Array::New(m_isolate, m_size);
  }
  else if (PyList_Check(m_items.ptr()))
  {
    m_size = PyList_GET_SIZE(m_items.ptr());
    array = v8::Array::New(m_isolate, m_size);

    for (Py_ssize_t i=0; i< (Py_ssize_t) m_size; i++)
    {
      array->Set(v8::Uint32::New(m_isolate, i), CPythonObject::Wrap(py::object(py::handle<>(py::borrowed(PyList_GET_ITEM(m_items.ptr(), i)))), m_isolate));
    }
  }
  else if (PyTuple_Check(m_items.ptr()))
  {
    m_size = PyTuple_GET_SIZE(m_items.ptr());
    array = v8::Array::New(m_isolate, m_size);

    for (Py_ssize_t i=0; i< (Py_ssize_t) m_size; i++)
    {
      array->Set(v8::Uint32::New(m_isolate, i), CPythonObject::Wrap(py::object(py::handle<>(py::borrowed(PyTuple_GET_ITEM(m_items.ptr(), i)))), m_isolate));
    }
  }
  else if (PyGen_Check(m_items.ptr()))
  {
    array = v8::Array::New(m_isolate);

    py::object iter(py::handle<>(::PyObject_GetIter(m_items.ptr())));

    m_size = 0;
    PyObject *item = NULL;

    while (NULL != (item = ::PyIter_Next(iter.ptr())))
    {
      array->Set(v8::Uint32::New(m_isolate, m_size++), CPythonObject::Wrap(py::object(py::handle<>(py::borrowed(item))), m_isolate));
    }
  }

  m_obj.Reset(m_isolate, array);
}
size_t CJavascriptArray::Length(void)
{
  CHECK_V8_CONTEXT(m_isolate);

  LazyConstructor();

  v8::HandleScope handle_scope(m_isolate);

  return v8::Handle<v8::Array>::Cast(Object())->Length();
}
py::object CJavascriptArray::GetItem(py::object key)
{
#ifdef SUPPORT_PROBES
  if (WRAPPER_JS_ARRAY_GETITEM_ENABLED()) {
    WRAPPER_JS_ARRAY_GETITEM(&m_obj, key.ptr());
  }
#endif

  CHECK_V8_CONTEXT(m_isolate);

  LazyConstructor();

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  if (PySlice_Check(key.ptr()))
  {
    Py_ssize_t arrayLen = v8::Handle<v8::Array>::Cast(Object())->Length();
    Py_ssize_t start, stop, step, sliceLen;

    if (0 == ::PySlice_GetIndicesEx(PySlice_Cast(key.ptr()), arrayLen, &start, &stop, &step, &sliceLen))
    {
      py::list slice;

      for (Py_ssize_t idx=start; idx<stop; idx+=step)
      {
        v8::Handle<v8::Value> value = Object()->Get(v8::Integer::New(m_isolate, (uint32_t) idx));

        if (value.IsEmpty()) CJavascriptException::ThrowIf(m_isolate, try_catch);

        slice.append(CJavascriptObject::Wrap(value, m_isolate, Object()));
      }

      return slice;
    }
  }
  else if (PyInt_Check(key.ptr()) || PyLong_Check(key.ptr()))
  {
    uint32_t idx = PyInt_Check(key.ptr()) ? (uint32_t) ::PyInt_AsUnsignedLongMask(key.ptr()) : (uint32_t) ::PyLong_AsUnsignedLong(key.ptr());

    if (!Object()->Has(idx)) return py::object();

    v8::Handle<v8::Value> value = Object()->Get(v8::Integer::New(m_isolate, idx));

    if (value.IsEmpty()) CJavascriptException::ThrowIf(m_isolate, try_catch);

    return CJavascriptObject::Wrap(value, m_isolate, Object());
  }

  throw CJavascriptException("list indices must be integers", ::PyExc_TypeError);
}

py::object CJavascriptArray::SetItem(py::object key, py::object value)
{
#ifdef SUPPORT_PROBES
  if (WRAPPER_JS_ARRAY_SETITEM_ENABLED()) {
    WRAPPER_JS_ARRAY_SETITEM(&m_obj, key.ptr(), value.ptr());
  }
#endif

  CHECK_V8_CONTEXT(m_isolate);

  LazyConstructor();

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  if (PySlice_Check(key.ptr()))
  {
    PyObject *values = ::PySequence_Fast(value.ptr(), "can only assign an iterable");

    if (values)
    {
      Py_ssize_t itemSize = PySequence_Fast_GET_SIZE(value.ptr());
      PyObject **items = PySequence_Fast_ITEMS(value.ptr());

      Py_ssize_t arrayLen = v8::Handle<v8::Array>::Cast(Object())->Length();
      Py_ssize_t start, stop, step, sliceLen;

      if (0 == ::PySlice_GetIndicesEx(PySlice_Cast(key.ptr()), arrayLen, &start, &stop, &step, &sliceLen))
      {
        if (itemSize != sliceLen)
        {
          v8i::Handle<v8i::JSArray> array = v8::Utils::OpenHandle(*Object());

          array->set_length(v8i::Smi::FromInt(arrayLen - sliceLen + itemSize));

          if (itemSize < sliceLen)
          {
            Py_ssize_t diff = sliceLen - itemSize;

            for (Py_ssize_t idx=start+itemSize; idx<arrayLen-diff; idx++)
            {
              Object()->Set(idx, Object()->Get((uint32_t) (idx + diff)));
            }
            for (Py_ssize_t idx=arrayLen-1; idx >arrayLen-diff-1; idx--)
            {
              Object()->Delete((uint32_t) idx);
            }
          }
          else if (itemSize > sliceLen)
          {
            Py_ssize_t diff = itemSize - sliceLen;

            for (Py_ssize_t idx=arrayLen+diff-1; idx>stop-1; idx--)
            {
              Object()->Set(idx, Object()->Get((uint32_t) (idx - diff)));
            }
          }
        }

        for (Py_ssize_t idx=0; idx<itemSize; idx++)
        {
          Object()->Set((uint32_t) (start + idx), CPythonObject::Wrap(py::object(py::handle<>(py::borrowed(items[idx]))), m_isolate));
        }
      }
    }
  }
  else if (PyInt_Check(key.ptr()) || PyLong_Check(key.ptr()))
  {
    uint32_t idx = PyInt_Check(key.ptr()) ? (uint32_t) ::PyInt_AsUnsignedLongMask(key.ptr()) : (uint32_t) ::PyLong_AsUnsignedLong(key.ptr());

    if (!Object()->Set(v8::Integer::New(m_isolate, idx), CPythonObject::Wrap(value, m_isolate)))
      CJavascriptException::ThrowIf(m_isolate, try_catch);
  }

  return value;
}
py::object CJavascriptArray::DelItem(py::object key)
{
#ifdef SUPPORT_PROBES
  if (WRAPPER_JS_ARRAY_DELITEM_ENABLED()) {
    WRAPPER_JS_ARRAY_DELITEM(&m_obj, key.ptr());
  }
#endif

  CHECK_V8_CONTEXT(m_isolate);

  LazyConstructor();

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  if (PySlice_Check(key.ptr()))
  {
    Py_ssize_t arrayLen = v8::Handle<v8::Array>::Cast(Object())->Length();
    Py_ssize_t start, stop, step, sliceLen;

    if (0 == ::PySlice_GetIndicesEx(PySlice_Cast(key.ptr()), arrayLen, &start, &stop, &step, &sliceLen))
    {
      for (Py_ssize_t idx=stop; idx<arrayLen; idx++)
      {
        Object()->Set((uint32_t) (idx - sliceLen), Object()->Get((uint32_t) idx));
      }

      for (Py_ssize_t idx=arrayLen-1; idx>arrayLen-sliceLen-1; idx--)
      {
        Object()->Delete((uint32_t)idx);
      }

      v8i::Handle<v8i::JSArray> array = v8::Utils::OpenHandle(*Object());

      array->set_length(v8i::Smi::FromInt(arrayLen - sliceLen));
    }

    return py::object();
  }
  else if (PyInt_Check(key.ptr()) || PyLong_Check(key.ptr()))
  {
    uint32_t idx = PyInt_Check(key.ptr()) ? (uint32_t) ::PyInt_AsUnsignedLongMask(key.ptr()) : (uint32_t) ::PyLong_AsUnsignedLong(key.ptr());

    py::object value;

    if (Object()->Has(idx))
      value = CJavascriptObject::Wrap(Object()->Get(v8::Integer::New(m_isolate, idx)), m_isolate, Object());

    if (!Object()->Delete(idx))
      CJavascriptException::ThrowIf(m_isolate, try_catch);

    return value;
  }

  throw CJavascriptException("list indices must be integers", ::PyExc_TypeError);
}

bool CJavascriptArray::Contains(py::object item)
{
  CHECK_V8_CONTEXT(m_isolate);

  LazyConstructor();

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  for (size_t i=0; i<Length(); i++)
  {
    if (Object()->Has(i))
    {
      v8::Handle<v8::Value> value = Object()->Get(v8::Integer::New(m_isolate, i));

      if (try_catch.HasCaught()) CJavascriptException::ThrowIf(m_isolate, try_catch);

      if (item == CJavascriptObject::Wrap(value, m_isolate, Object()))
      {
        return true;
      }
    }
  }

  if (try_catch.HasCaught()) CJavascriptException::ThrowIf(m_isolate, try_catch);

  return false;
}

py::object CJavascriptFunction::CallWithArgs(py::tuple args, py::dict kwds)
{
  size_t argc = ::PyTuple_Size(args.ptr());

  if (argc == 0) throw CJavascriptException("missed self argument", ::PyExc_TypeError);

  py::object self = args[0];
  py::extract<CJavascriptFunction&> extractor(self);

  if (!extractor.check()) throw CJavascriptException("missed self argument", ::PyExc_TypeError);

  CJavascriptFunction& func = extractor();
  
  CHECK_V8_CONTEXT(func.m_isolate);
  
  v8::HandleScope handle_scope(func.m_isolate);
  v8::TryCatch try_catch;

  py::list argv(args.slice(1, py::_));

  return func.Call(func.Self(), argv, kwds);
}

py::object CJavascriptFunction::Call(v8::Handle<v8::Object> self, py::list args, py::dict kwds)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::TryCatch try_catch;

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  size_t args_count = ::PyList_Size(args.ptr()), kwds_count = ::PyMapping_Size(kwds.ptr());

  std::vector< v8::Handle<v8::Value> > params(args_count + kwds_count);

  for (size_t i=0; i<args_count; i++)
  {
    params[i] = CPythonObject::Wrap(args[i], m_isolate);
  }

  py::list values = kwds.values();

  for (size_t i=0; i<kwds_count; i++)
  {
    params[args_count+i] = CPythonObject::Wrap(values[i], m_isolate);
  }

  v8::Handle<v8::Value> result;

  Py_BEGIN_ALLOW_THREADS

  result = func->Call(
    self.IsEmpty() ? m_isolate->GetCurrentContext()->Global() : self,
    params.size(), params.empty() ? NULL : &params[0]);

  Py_END_ALLOW_THREADS

  if (result.IsEmpty()) CJavascriptException::ThrowIf(m_isolate, try_catch);

  return CJavascriptObject::Wrap(result, m_isolate);
}

py::object CJavascriptFunction::CreateWithArgs(CJavascriptFunctionPtr proto, py::tuple args, py::dict kwds, CIsolatePtr isolate_)
{
  v8::Isolate* isolate = isolate_.get() ? isolate_->GetIsolate() : v8::Isolate::GetCurrent();

  CHECK_V8_CONTEXT(isolate);

  v8::HandleScope handle_scope(isolate);

  if (proto->Object().IsEmpty())
    throw CJavascriptException("Object prototype may only be an Object", ::PyExc_TypeError);

  v8::TryCatch try_catch;

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(proto->Object());

  size_t args_count = ::PyTuple_Size(args.ptr());

  std::vector< v8::Handle<v8::Value> > params(args_count);

  for (size_t i=0; i<args_count; i++)
  {
    params[i] = CPythonObject::Wrap(args[i], isolate);
  }

  v8::Handle<v8::Object> result;

  Py_BEGIN_ALLOW_THREADS

  result = func->NewInstance(params.size(), params.empty() ? NULL : &params[0]);

  Py_END_ALLOW_THREADS

  if (result.IsEmpty()) CJavascriptException::ThrowIf(isolate, try_catch);

  size_t kwds_count = ::PyMapping_Size(kwds.ptr());
  py::list items = kwds.items();

  for (size_t i=0; i<kwds_count; i++)
  {
    py::tuple item(items[i]);

    py::str key(item[0]);
    py::object value = item[1];

    result->Set(ToString(key, isolate), CPythonObject::Wrap(value, isolate));
  }

  return CJavascriptObject::Wrap(result, isolate);
}

py::object CJavascriptFunction::ApplyJavascript(CJavascriptObjectPtr self, py::list args, py::dict kwds)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);
  
  return Call(self->Object(), args, kwds);
}

py::object CJavascriptFunction::ApplyPython(py::object self, py::list args, py::dict kwds)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  return Call(CPythonObject::Wrap(self, m_isolate)->ToObject(), args, kwds);
}

py::object CJavascriptFunction::Invoke(py::list args, py::dict kwds)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  return Call(Self(), args, kwds);
}

const std::string CJavascriptFunction::GetName(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  v8::String::Utf8Value name(v8::Handle<v8::String>::Cast(func->GetName()));

  return std::string(*name, name.length());
}

void CJavascriptFunction::SetName(const std::string& name)
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  func->SetName(v8::String::NewFromUtf8(m_isolate, name.c_str(), v8::String::kNormalString, name.size()));
}

int CJavascriptFunction::GetLineNumber(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  return func->GetScriptLineNumber();
}
int CJavascriptFunction::GetColumnNumber(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  return func->GetScriptColumnNumber();
}
const std::string CJavascriptFunction::GetResourceName(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  v8::String::Utf8Value name(v8::Handle<v8::String>::Cast(func->GetScriptOrigin().ResourceName()));

  return std::string(*name, name.length());
}
const std::string CJavascriptFunction::GetInferredName(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  v8::String::Utf8Value name(v8::Handle<v8::String>::Cast(func->GetInferredName()));

  return std::string(*name, name.length());
}
int CJavascriptFunction::GetLineOffset(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  return func->GetScriptOrigin().ResourceLineOffset()->Value();
}
int CJavascriptFunction::GetColumnOffset(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  v8::Handle<v8::Function> func = v8::Handle<v8::Function>::Cast(Object());

  return func->GetScriptOrigin().ResourceColumnOffset()->Value();
}
py::object CJavascriptFunction::GetOwner(void) const
{
  CHECK_V8_CONTEXT(m_isolate);

  v8::HandleScope handle_scope(m_isolate);

  return CJavascriptObject::Wrap(Self(), m_isolate);
}

#ifdef SUPPORT_TRACE_LIFECYCLE

ObjectTracer::ObjectTracer(v8::Handle<v8::Value> handle, py::object *object)
  : m_handle(v8::Isolate::GetCurrent(), handle),
    m_object(object), m_living(GetLivingMapping())
{
}

ObjectTracer::~ObjectTracer()
{
  if (!m_handle.IsEmpty())
  {
    assert(m_handle.IsNearDeath());

    Dispose();

    m_living->erase(m_object->ptr());
  }
}

void ObjectTracer::Dispose(void)
{
  m_handle.ClearWeak();
  m_handle.Reset();
}

ObjectTracer& ObjectTracer::Trace(v8::Handle<v8::Value> handle, py::object *object)
{
  std::auto_ptr<ObjectTracer> tracer(new ObjectTracer(handle, object));

  tracer->Trace();

  return *tracer.release();
}

void ObjectTracer::Trace(void)
{
  m_handle.SetWeak(this, WeakCallback);

  m_living->insert(std::make_pair(m_object->ptr(), this));
}

void ObjectTracer::WeakCallback(const v8::WeakCallbackData<v8::Value, ObjectTracer>& data)
{
  std::auto_ptr<ObjectTracer> tracer(data.GetParameter());

  assert(data.GetValue() == tracer->m_handle);
}

LivingMap * ObjectTracer::GetLivingMapping(void)
{
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());

  v8::Handle<v8::Context> ctxt = v8::Isolate::GetCurrent()->GetCurrentContext();

  v8::Handle<v8::Value> value = ctxt->Global()->GetHiddenValue(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "__living__"));

  if (!value.IsEmpty())
  {
    LivingMap *living = (LivingMap *) v8::External::Cast(*value)->Value();

    if (living) return living;
  }

  std::auto_ptr<LivingMap> living(new LivingMap());

  ctxt->Global()->SetHiddenValue(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "__living__"), v8::External::New(v8::Isolate::GetCurrent(), living.get()));

  ContextTracer::Trace(ctxt, living.get());

  return living.release();
}

v8::Handle<v8::Value> ObjectTracer::FindCache(py::object obj)
{
  LivingMap *living = GetLivingMapping();

  if (living)
  {
    LivingMap::const_iterator it = living->find(obj.ptr());

    if (it != living->end())
    {
      return v8::Local<v8::Value>::New(v8::Isolate::GetCurrent(), it->second->m_handle);
    }
  }

  return v8::Handle<v8::Value>();
}

ContextTracer::ContextTracer(v8::Handle<v8::Context> ctxt, LivingMap *living)
  : m_ctxt(v8::Isolate::GetCurrent(), ctxt), m_living(living)
{
}

ContextTracer::~ContextTracer(void)
{
  Context()->Global()->DeleteHiddenValue(v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), "__living__"));

  for (LivingMap::const_iterator it = m_living->begin(); it != m_living->end(); it++)
  {
    std::auto_ptr<ObjectTracer> tracer(it->second);

    tracer->Dispose();
  }
}

void ContextTracer::WeakCallback(const v8::WeakCallbackData<v8::Context, ContextTracer>& data)
{
  delete data.GetParameter();
}

void ContextTracer::Trace(v8::Handle<v8::Context> ctxt, LivingMap *living)
{
  ContextTracer *tracer = new ContextTracer(ctxt, living);

  tracer->Trace();
}

void ContextTracer::Trace(void)
{
  m_ctxt.SetWeak(this, WeakCallback);
}

#endif // SUPPORT_TRACE_LIFECYCLE
