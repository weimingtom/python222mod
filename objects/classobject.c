//20170410
#include "python.h"
#include "structmember.h"

#define TP_DESCR_GET(t) \
    (PyType_HasFeature(t, Py_TPFLAGS_HAVE_CLASS) ? (t)->tp_descr_get : NULL)

static PyObject *class_lookup(PyClassObject *, PyObject *,
			      PyClassObject **);
static PyObject *instance_getattr1(PyInstanceObject *, PyObject *);
static PyObject *instance_getattr2(PyInstanceObject *, PyObject *);

static PyObject *getattrstr, *setattrstr, *delattrstr;

PyObject *PyClass_New(PyObject *bases, PyObject *dict, PyObject *name)
{
	PyClassObject *op, *dummy;
	static PyObject *docstr, *modstr, *namestr;
	if (docstr == NULL) 
	{
		docstr = PyString_InternFromString("__doc__");
		if (docstr == NULL)
		{
			return NULL;
		}
	}
	if (modstr == NULL) 
	{
		modstr= PyString_InternFromString("__module__");
		if (modstr == NULL)
		{
			return NULL;
		}
	}
	if (namestr == NULL) 
	{
		namestr= PyString_InternFromString("__name__");
		if (namestr == NULL)
		{
			return NULL;
		}
	}
	if (name == NULL || !PyString_Check(name)) 
	{
		PyErr_SetString(PyExc_TypeError,
				"PyClass_New: name must be a string");
		return NULL;
	}
	if (dict == NULL || !PyDict_Check(dict)) 
	{
		PyErr_SetString(PyExc_TypeError,
				"PyClass_New: dict must be a dictionary");
		return NULL;
	}
	if (PyDict_GetItem(dict, docstr) == NULL) 
	{
		if (PyDict_SetItem(dict, docstr, Py_None) < 0)
		{
			return NULL;
		}
	}
	if (PyDict_GetItem(dict, modstr) == NULL) 
	{
		PyObject *globals = PyEval_GetGlobals();
		if (globals != NULL) 
		{
			PyObject *modname = PyDict_GetItem(globals, namestr);
			if (modname != NULL) 
			{
				if (PyDict_SetItem(dict, modstr, modname) < 0)
				{
					return NULL;
				}
			}
		}
	}
	if (bases == NULL) 
	{
		bases = PyTuple_New(0);
		if (bases == NULL)
		{
			return NULL;
		}
	}
	else 
	{
		int i, n;
		PyObject *base;
		if (!PyTuple_Check(bases)) 
		{
			PyErr_SetString(PyExc_TypeError,
					"PyClass_New: bases must be a tuple");
			return NULL;
		}
		n = PyTuple_Size(bases);
		for (i = 0; i < n; i++) 
		{
			base = PyTuple_GET_ITEM(bases, i);
			if (!PyClass_Check(base)) 
			{
				if (PyCallable_Check(
					(PyObject *) base->ob_type))
				{
					return PyObject_CallFunction(
						(PyObject *) base->ob_type,
						"OOO",
						name,
						bases,
						dict);
				}
				PyErr_SetString(PyExc_TypeError,
					"PyClass_New: base must be a class");
				return NULL;
			}
		}
		Py_INCREF(bases);
	}
	op = PyObject_GC_New(PyClassObject, &PyClass_Type);
	if (op == NULL) 
	{
		Py_DECREF(bases);
		return NULL;
	}
	op->cl_bases = bases;
	Py_INCREF(dict);
	op->cl_dict = dict;
	Py_XINCREF(name);
	op->cl_name = name;
	if (getattrstr == NULL) 
	{
		getattrstr = PyString_InternFromString("__getattr__");
		setattrstr = PyString_InternFromString("__setattr__");
		delattrstr = PyString_InternFromString("__delattr__");
	}
	op->cl_getattr = class_lookup(op, getattrstr, &dummy);
	op->cl_setattr = class_lookup(op, setattrstr, &dummy);
	op->cl_delattr = class_lookup(op, delattrstr, &dummy);
	Py_XINCREF(op->cl_getattr);
	Py_XINCREF(op->cl_setattr);
	Py_XINCREF(op->cl_delattr);
	_PyObject_GC_TRACK(op);
	return (PyObject *) op;
}

PyObject *PyMethod_Function(PyObject *im)
{
	if (!PyMethod_Check(im)) 
	{
		PyErr_BadInternalCall();
		return NULL;
	}
	return ((PyMethodObject *)im)->im_func;
}

PyObject *PyMethod_Self(PyObject *im)
{
	if (!PyMethod_Check(im)) 
	{
		PyErr_BadInternalCall();
		return NULL;
	}
	return ((PyMethodObject *)im)->im_self;
}

PyObject *PyMethod_Class(PyObject *im)
{
	if (!PyMethod_Check(im)) 
	{
		PyErr_BadInternalCall();
		return NULL;
	}
	return ((PyMethodObject *)im)->im_class;
}

static PyObject *class_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *name, *bases, *dict;
	static char *kwlist[] = {"name", "bases", "dict", 0};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "SOO", kwlist,
					 &name, &bases, &dict))
	{
		return NULL;
	}
	return PyClass_New(bases, dict, name);
}

static void class_dealloc(PyClassObject *op)
{
	_PyObject_GC_UNTRACK(op);
	Py_DECREF(op->cl_bases);
	Py_DECREF(op->cl_dict);
	Py_XDECREF(op->cl_name);
	Py_XDECREF(op->cl_getattr);
	Py_XDECREF(op->cl_setattr);
	Py_XDECREF(op->cl_delattr);
	PyObject_GC_Del(op);
}

static PyObject *class_lookup(PyClassObject *cp, PyObject *name, PyClassObject **pclass)
{
	int i, n;
	PyObject *value = PyDict_GetItem(cp->cl_dict, name);
	if (value != NULL) 
	{
		*pclass = cp;
		return value;
	}
	n = PyTuple_Size(cp->cl_bases);
	for (i = 0; i < n; i++) 
	{
		PyObject *v = class_lookup(
			(PyClassObject *)
			PyTuple_GetItem(cp->cl_bases, i), name, pclass);
		if (v != NULL)
		{
			return v;
		}
	}
	return NULL;
}

static PyObject *class_getattr(PyClassObject *op, PyObject *name)
{
	PyObject *v;
	char *sname = PyString_AsString(name);
	PyClassObject *class_;
	descrgetfunc f;

	if (sname[0] == '_' && sname[1] == '_') 
	{
		if (strcmp(sname, "__dict__") == 0) 
		{
			if (PyEval_GetRestricted()) 
			{
				PyErr_SetString(PyExc_RuntimeError,
			   "class.__dict__ not accessible in restricted mode");
				return NULL;
			}
			Py_INCREF(op->cl_dict);
			return op->cl_dict;
		}
		if (strcmp(sname, "__bases__") == 0) 
		{
			Py_INCREF(op->cl_bases);
			return op->cl_bases;
		}
		if (strcmp(sname, "__name__") == 0) 
		{
			if (op->cl_name == NULL)
			{
				v = Py_None;
			}
			else
			{
				v = op->cl_name;
			}
			Py_INCREF(v);
			return v;
		}
	}
	v = class_lookup(op, name, &class_);
	if (v == NULL) 
	{
		PyErr_Format(PyExc_AttributeError,
			     "class %.50s has no attribute '%.400s'",
			     PyString_AS_STRING(op->cl_name), sname);
		return NULL;
	}
	f = TP_DESCR_GET(v->ob_type);
	if (f == NULL)
	{
		Py_INCREF(v);
	}
	else
	{
		v = f(v, (PyObject *)NULL, (PyObject *)op);
	}
	return v;
}

static void set_slot(PyObject **slot, PyObject *v)
{
	PyObject *temp = *slot;
	Py_XINCREF(v);
	*slot = v;
	Py_XDECREF(temp);
}

static void set_attr_slots(PyClassObject *c)
{
	PyClassObject *dummy;

	set_slot(&c->cl_getattr, class_lookup(c, getattrstr, &dummy));
	set_slot(&c->cl_setattr, class_lookup(c, setattrstr, &dummy));
	set_slot(&c->cl_delattr, class_lookup(c, delattrstr, &dummy));
}

static char *set_dict(PyClassObject *c, PyObject *v)
{
	if (v == NULL || !PyDict_Check(v))
	{
		return "__dict__ must be a dictionary object";
	}
	set_slot(&c->cl_dict, v);
	set_attr_slots(c);
	return "";
}

static char *set_bases(PyClassObject *c, PyObject *v)
{
	int i, n;

	if (v == NULL || !PyTuple_Check(v))
	{
		return "__bases__ must be a tuple object";
	}
	n = PyTuple_Size(v);
	for (i = 0; i < n; i++) 
	{
		PyObject *x = PyTuple_GET_ITEM(v, i);
		if (!PyClass_Check(x))
		{
			return "__bases__ items must be classes";
		}
		if (PyClass_IsSubclass(x, (PyObject *)c))
		{
			return "a __bases__ item causes an inheritance cycle";
		}
	}
	set_slot(&c->cl_bases, v);
	set_attr_slots(c);
	return "";
}

static char *set_name(PyClassObject *c, PyObject *v)
{
	if (v == NULL || !PyString_Check(v))
	{
		return "__name__ must be a string object";
	}
	if (strlen(PyString_AS_STRING(v)) != (size_t)PyString_GET_SIZE(v))
	{
		return "__name__ must not contain null bytes";
	}
	set_slot(&c->cl_name, v);
	return "";
}

static int class_setattr(PyClassObject *op, PyObject *name, PyObject *v)
{
	char *sname;
	if (PyEval_GetRestricted()) 
	{
		PyErr_SetString(PyExc_RuntimeError,
			   "classes are read-only in restricted mode");
		return -1;
	}
	sname = PyString_AsString(name);
	if (sname[0] == '_' && sname[1] == '_') 
	{
		int n = PyString_Size(name);
		if (sname[n-1] == '_' && sname[n-2] == '_') 
		{
			char *err = NULL;
			if (strcmp(sname, "__dict__") == 0)
			{
				err = set_dict(op, v);
			}
			else if (strcmp(sname, "__bases__") == 0)
			{
				err = set_bases(op, v);
			}
			else if (strcmp(sname, "__name__") == 0)
			{
				err = set_name(op, v);
			}
			else if (strcmp(sname, "__getattr__") == 0)
			{
				set_slot(&op->cl_getattr, v);
			}
			else if (strcmp(sname, "__setattr__") == 0)
			{
				set_slot(&op->cl_setattr, v);
			}
			else if (strcmp(sname, "__delattr__") == 0)
			{
				set_slot(&op->cl_delattr, v);
			}
			if (err != NULL) 
			{
				if (*err == '\0')
				{
					return 0;
				}
				PyErr_SetString(PyExc_TypeError, err);
				return -1;
			}
		}
	}
	if (v == NULL) 
	{
		int rv = PyDict_DelItem(op->cl_dict, name);
		if (rv < 0)
		{
			PyErr_Format(PyExc_AttributeError,
				     "class %.50s has no attribute '%.400s'",
				     PyString_AS_STRING(op->cl_name), sname);
		}
		return rv;
	}
	else
	{
		return PyDict_SetItem(op->cl_dict, name, v);
	}
}

static PyObject *class_repr(PyClassObject *op)
{
	PyObject *mod = PyDict_GetItemString(op->cl_dict, "__module__");
	char *name;
	if (op->cl_name == NULL || !PyString_Check(op->cl_name))
	{
		name = "?";
	}
	else
	{
		name = PyString_AsString(op->cl_name);
	}
	if (mod == NULL || !PyString_Check(mod))
	{
		return PyString_FromFormat("<class ?.%s at %p>", name, op);
	}
	else
	{
		return PyString_FromFormat("<class %s.%s at %p>",
					   PyString_AsString(mod),
					   name, op);
	}
}

static PyObject *class_str(PyClassObject *op)
{
	PyObject *mod = PyDict_GetItemString(op->cl_dict, "__module__");
	PyObject *name = op->cl_name;
	PyObject *res;
	int m, n;

	if (name == NULL || !PyString_Check(name))
	{
		return class_repr(op);
	}
	if (mod == NULL || !PyString_Check(mod)) 
	{
		Py_INCREF(name);
		return name;
	}
	m = PyString_Size(mod);
	n = PyString_Size(name);
	res = PyString_FromStringAndSize((char *)NULL, m+1+n);
	if (res != NULL) 
	{
		char *s = PyString_AsString(res);
		memcpy(s, PyString_AsString(mod), m);
		s += m;
		*s++ = '.';
		memcpy(s, PyString_AsString(name), n);
	}
	return res;
}

static int class_traverse(PyClassObject *o, visitproc visit, void *arg)
{
	int err;
	if (o->cl_bases) 
	{
		err = visit(o->cl_bases, arg);
		if (err)
		{
			return err;
		}
	}
	if (o->cl_dict) 
	{
		err = visit(o->cl_dict, arg);
		if (err)
		{
			return err;
		}
	}
	if (o->cl_name) 
	{
		err = visit(o->cl_name, arg);
		if (err)
		{
			return err;
		}
	}
	if (o->cl_getattr) 
	{
		err = visit(o->cl_getattr, arg);
		if (err)
		{
			return err;
		}
	}
	if (o->cl_setattr) 
	{
		err = visit(o->cl_setattr, arg);
		if (err)
		{
			return err;
		}
	}
	if (o->cl_delattr) 
	{
		err = visit(o->cl_delattr, arg);
		if (err)
		{
			return err;
		}
	}
	return 0;
}

PyTypeObject PyClass_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"class",
	sizeof(PyClassObject),
	0,
	(destructor)class_dealloc,		
	0,				
	0,					
	0,					
	0,					
	(reprfunc)class_repr,	
	0,					
	0,					
	0,					
	0,					
	PyInstance_New,			
	(reprfunc)class_str,	
	(getattrofunc)class_getattr,	
	(setattrofunc)class_setattr,	
	0,					
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
	0,				
	(traverseproc)class_traverse,		
 	0,					
	0,					
	0,					
	0,					
	0,					
	0,					
	0,					
	0,					
	0,					
	0,					
	0,					
	0,					
	0,				
	0,				
	0,				
	class_new,		
};

int PyClass_IsSubclass(PyObject *class_, PyObject *base)
{
	int i, n;
	PyClassObject *cp;
	if (class_ == base)
	{
		return 1;
	}
	if (class_ == NULL || !PyClass_Check(class_))
	{
		return 0;
	}
	cp = (PyClassObject *)class_;
	n = PyTuple_Size(cp->cl_bases);
	for (i = 0; i < n; i++) 
	{
		if (PyClass_IsSubclass(PyTuple_GetItem(cp->cl_bases, i), base))
		{
			return 1;
		}
	}
	return 0;
}

PyObject *PyInstance_NewRaw(PyObject *klass, PyObject *dict)
{
	PyInstanceObject *inst;

	if (!PyClass_Check(klass)) 
	{
		PyErr_BadInternalCall();
		return NULL;
	}
	if (dict == NULL) 
	{
		dict = PyDict_New();
		if (dict == NULL)
		{
			return NULL;
		}
	}
	else 
	{
		if (!PyDict_Check(dict)) 
		{
			PyErr_BadInternalCall();
			return NULL;
		}
		Py_INCREF(dict);
	}
	inst = PyObject_GC_New(PyInstanceObject, &PyInstance_Type);
	if (inst == NULL) 
	{
		Py_DECREF(dict);
		return NULL;
	}
	inst->in_weakreflist = NULL;
	Py_INCREF(klass);
	inst->in_class = (PyClassObject *)klass;
	inst->in_dict = dict;
	_PyObject_GC_TRACK(inst);
	return (PyObject *)inst;
}

PyObject *PyInstance_New(PyObject *klass, PyObject *arg, PyObject *kw)
{
	PyInstanceObject *inst;
	PyObject *init;
	static PyObject *initstr;

	inst = (PyInstanceObject *) PyInstance_NewRaw(klass, NULL);
	if (inst == NULL)
	{
		return NULL;
	}
	if (initstr == NULL)
	{
		initstr = PyString_InternFromString("__init__");
	}
	init = instance_getattr2(inst, initstr);
	if (init == NULL) 
	{
		if ((arg != NULL && (!PyTuple_Check(arg) ||
				     PyTuple_Size(arg) != 0))
		    || (kw != NULL && (!PyDict_Check(kw) ||
				      PyDict_Size(kw) != 0))) 
		{
			PyErr_SetString(PyExc_TypeError,
				   "this constructor takes no arguments");
			Py_DECREF(inst);
			inst = NULL;
		}
	}
	else 
	{
		PyObject *res = PyEval_CallObjectWithKeywords(init, arg, kw);
		Py_DECREF(init);
		if (res == NULL) 
		{
			Py_DECREF(inst);
			inst = NULL;
		}
		else 
		{
			if (res != Py_None) 
			{
				PyErr_SetString(PyExc_TypeError,
					   "__init__() should return None");
				Py_DECREF(inst);
				inst = NULL;
			}
			Py_DECREF(res);
		}
	}
	return (PyObject *)inst;
}

static void instance_dealloc(PyInstanceObject *inst)
{
	PyObject *error_type, *error_value, *error_traceback;
	PyObject *del;
	static PyObject *delstr;
#ifdef _DEBUG
	extern long _Py_RefTotal;
#endif
	_PyObject_GC_UNTRACK(inst);
	if (inst->in_weakreflist != NULL)
	{
		PyObject_ClearWeakRefs((PyObject *) inst);
	}

#ifdef _DEBUG
	inst->ob_type = &PyInstance_Type;
	_Py_NewReference((PyObject *)inst);
#else
	Py_INCREF(inst);
#endif

	PyErr_Fetch(&error_type, &error_value, &error_traceback);
	if (delstr == NULL)
	{
		delstr = PyString_InternFromString("__del__");
	}
	if ((del = instance_getattr2(inst, delstr)) != NULL) 
	{
		PyObject *res = PyEval_CallObject(del, (PyObject *)NULL);
		if (res == NULL)
		{
			PyErr_WriteUnraisable(del);
		}
		else
		{
			Py_DECREF(res);
		}
		Py_DECREF(del);
	}
	PyErr_Restore(error_type, error_value, error_traceback);
#ifdef _DEBUG
	_Py_RefTotal--;
#endif
	if (--inst->ob_refcnt > 0) 
	{
		_PyObject_GC_TRACK(inst);
		return;
	}
#ifdef _DEBUG
	_Py_ForgetReference((PyObject *)inst);
#endif
	Py_DECREF(inst->in_class);
	Py_XDECREF(inst->in_dict);
	PyObject_GC_Del(inst);
}

static PyObject *instance_getattr1(PyInstanceObject *inst, PyObject *name)
{
	PyObject *v;
	char *sname = PyString_AsString(name);
	if (sname[0] == '_' && sname[1] == '_') 
	{
		if (strcmp(sname, "__dict__") == 0) 
		{
			if (PyEval_GetRestricted()) 
			{
				PyErr_SetString(PyExc_RuntimeError,
					"instance.__dict__ not accessible in restricted mode");
				return NULL;
			}
			Py_INCREF(inst->in_dict);
			return inst->in_dict;
		}
		if (strcmp(sname, "__class__") == 0) 
		{
			Py_INCREF(inst->in_class);
			return (PyObject *)inst->in_class;
		}
	}
	v = instance_getattr2(inst, name);
	if (v == NULL) 
	{
		PyErr_Format(PyExc_AttributeError,
			     "%.50s instance has no attribute '%.400s'",
			     PyString_AS_STRING(inst->in_class->cl_name), sname);
	}
	return v;
}

static PyObject *instance_getattr2(PyInstanceObject *inst, PyObject *name)
{
	PyObject *v;
	PyClassObject *class_;
	descrgetfunc f;

	v = PyDict_GetItem(inst->in_dict, name);
	if (v != NULL) 
	{
		Py_INCREF(v);
		return v;
	}
	v = class_lookup(inst->in_class, name, &class_);
	if (v != NULL)
	{
		Py_INCREF(v);
		f = TP_DESCR_GET(v->ob_type);
		if (f != NULL) 
		{
			PyObject *w = f(v, (PyObject *)inst,
					(PyObject *)(inst->in_class));
			Py_DECREF(v);
			v = w;
		}
	}
	return v;
}

static PyObject *instance_getattr(PyInstanceObject *inst, PyObject *name)
{
	PyObject *func, *res;
	res = instance_getattr1(inst, name);
	if (res == NULL && (func = inst->in_class->cl_getattr) != NULL) 
	{
		PyObject *args;
		PyErr_Clear();
		args = Py_BuildValue("(OO)", inst, name);
		if (args == NULL)
		{
			return NULL;
		}
		res = PyEval_CallObject(func, args);
		Py_DECREF(args);
	}
	return res;
}

static int instance_setattr1(PyInstanceObject *inst, PyObject *name, PyObject *v)
{
	if (v == NULL) 
	{
		int rv = PyDict_DelItem(inst->in_dict, name);
		if (rv < 0)
		{
			PyErr_Format(PyExc_AttributeError,
				     "%.50s instance has no attribute '%.400s'",
				     PyString_AS_STRING(inst->in_class->cl_name),
				     PyString_AS_STRING(name));
		}
		return rv;
	}
	else
	{
		return PyDict_SetItem(inst->in_dict, name, v);
	}
}

static int instance_setattr(PyInstanceObject *inst, PyObject *name, PyObject *v)
{
	PyObject *func, *args, *res, *tmp;
	char *sname = PyString_AsString(name);
	if (sname[0] == '_' && sname[1] == '_') 
	{
		int n = PyString_Size(name);
		if (sname[n-1] == '_' && sname[n-2] == '_') 
		{
			if (strcmp(sname, "__dict__") == 0) 
			{
				if (PyEval_GetRestricted()) 
				{
					PyErr_SetString(PyExc_RuntimeError,
						"__dict__ not accessible in restricted mode");
					return -1;
				}
				if (v == NULL || !PyDict_Check(v)) 
				{
				    PyErr_SetString(PyExc_TypeError,
				       "__dict__ must be set to a dictionary");
				    return -1;
				}
				tmp = inst->in_dict;
				Py_INCREF(v);
				inst->in_dict = v;
				Py_DECREF(tmp);
				return 0;
			}
			if (strcmp(sname, "__class__") == 0) 
			{
				if (PyEval_GetRestricted()) 
				{
					PyErr_SetString(PyExc_RuntimeError,
						"__class__ not accessible in restricted mode");
					return -1;
				}
				if (v == NULL || !PyClass_Check(v)) 
				{
					PyErr_SetString(PyExc_TypeError,
					   "__class__ must be set to a class");
					return -1;
				}
				tmp = (PyObject *)(inst->in_class);
				Py_INCREF(v);
				inst->in_class = (PyClassObject *)v;
				Py_DECREF(tmp);
				return 0;
			}
		}
	}
	if (v == NULL)
	{
		func = inst->in_class->cl_delattr;
	}
	else
	{
		func = inst->in_class->cl_setattr;
	}
	if (func == NULL)
	{
		return instance_setattr1(inst, name, v);
	}
	if (v == NULL)
	{
		args = Py_BuildValue("(OO)", inst, name);
	}
	else
	{
		args = Py_BuildValue("(OOO)", inst, name, v);
	}
	if (args == NULL)
	{
		return -1;
	}
	res = PyEval_CallObject(func, args);
	Py_DECREF(args);
	if (res == NULL)
	{
		return -1;
	}
	Py_DECREF(res);
	return 0;
}

static PyObject *instance_repr(PyInstanceObject *inst)
{
	PyObject *func;
	PyObject *res;
	static PyObject *reprstr;

	if (reprstr == NULL)
	{
		reprstr = PyString_InternFromString("__repr__");
	}
	func = instance_getattr(inst, reprstr);
	if (func == NULL) 
	{
		PyObject *classname = inst->in_class->cl_name;
		PyObject *mod = PyDict_GetItemString(
			inst->in_class->cl_dict, "__module__");
		char *cname;
		if (classname != NULL && PyString_Check(classname))
		{
			cname = PyString_AsString(classname);
		}
		else
		{
			cname = "?";
		}
		PyErr_Clear();
		if (mod == NULL || !PyString_Check(mod))
		{
			return PyString_FromFormat("<?.%s instance at %p>",
						   cname, inst);
		}
		else
		{
			return PyString_FromFormat("<%s.%s instance at %p>",
						   PyString_AsString(mod),
						   cname, inst);
		}
	}
	res = PyEval_CallObject(func, (PyObject *)NULL);
	Py_DECREF(func);
	return res;
}

static PyObject *instance_str(PyInstanceObject *inst)
{
	PyObject *func;
	PyObject *res;
	static PyObject *strstr;

	if (strstr == NULL)
	{
		strstr = PyString_InternFromString("__str__");
	}
	func = instance_getattr(inst, strstr);
	if (func == NULL) 
	{
		PyErr_Clear();
		return instance_repr(inst);
	}
	res = PyEval_CallObject(func, (PyObject *)NULL);
	Py_DECREF(func);
	return res;
}

static long instance_hash(PyInstanceObject *inst)
{
	PyObject *func;
	PyObject *res;
	long outcome;
	static PyObject *hashstr, *eqstr, *cmpstr;

	if (hashstr == NULL)
		hashstr = PyString_InternFromString("__hash__");
	func = instance_getattr(inst, hashstr);
	if (func == NULL) 
	{
		PyErr_Clear();
		if (eqstr == NULL)
		{
			eqstr = PyString_InternFromString("__eq__");
		}
		func = instance_getattr(inst, eqstr);
		if (func == NULL) 
		{
			PyErr_Clear();
			if (cmpstr == NULL)
			{
				cmpstr = PyString_InternFromString("__cmp__");
			}
			func = instance_getattr(inst, cmpstr);
			if (func == NULL) 
			{
				PyErr_Clear();
				return _Py_HashPointer(inst);
			}
		}
		PyErr_SetString(PyExc_TypeError, "unhashable instance");
		return -1;
	}
	res = PyEval_CallObject(func, (PyObject *)NULL);
	Py_DECREF(func);
	if (res == NULL)
	{
		return -1;
	}
	if (PyInt_Check(res)) 
	{
		outcome = PyInt_AsLong(res);
		if (outcome == -1)
		{
			outcome = -2;
		}
	}
	else 
	{
		PyErr_SetString(PyExc_TypeError,
				"__hash__() should return an int");
		outcome = -1;
	}
	Py_DECREF(res);
	return outcome;
}

static int instance_traverse(PyInstanceObject *o, visitproc visit, void *arg)
{
	int err;
	if (o->in_class) 
	{
		err = visit((PyObject *)(o->in_class), arg);
		if (err)
		{
			return err;
		}
	}
	if (o->in_dict) 
	{
		err = visit(o->in_dict, arg);
		if (err)
		{
			return err;
		}
	}
	return 0;
}

static PyObject *getitemstr, *setitemstr, *delitemstr, *lenstr;
static PyObject *iterstr, *nextstr;

static int instance_length(PyInstanceObject *inst)
{
	PyObject *func;
	PyObject *res;
	int outcome;

	if (lenstr == NULL)
	{
		lenstr = PyString_InternFromString("__len__");
	}
	func = instance_getattr(inst, lenstr);
	if (func == NULL)
	{
		return -1;
	}
	res = PyEval_CallObject(func, (PyObject *)NULL);
	Py_DECREF(func);
	if (res == NULL)
	{
		return -1;
	}
	if (PyInt_Check(res)) 
	{
		outcome = PyInt_AsLong(res);
		if (outcome < 0)
		{
			PyErr_SetString(PyExc_ValueError,
					"__len__() should return >= 0");
		}
	}
	else 
	{
		PyErr_SetString(PyExc_TypeError,
				"__len__() should return an int");
		outcome = -1;
	}
	Py_DECREF(res);
	return outcome;
}

static PyObject *instance_subscript(PyInstanceObject *inst, PyObject *key)
{
	PyObject *func;
	PyObject *arg;
	PyObject *res;

	if (getitemstr == NULL)
	{
		getitemstr = PyString_InternFromString("__getitem__");
	}
	func = instance_getattr(inst, getitemstr);
	if (func == NULL)
	{
		return NULL;
	}
	arg = Py_BuildValue("(O)", key);
	if (arg == NULL) 
	{
		Py_DECREF(func);
		return NULL;
	}
	res = PyEval_CallObject(func, arg);
	Py_DECREF(func);
	Py_DECREF(arg);
	return res;
}

static int instance_ass_subscript(PyInstanceObject *inst, PyObject *key, PyObject *value)
{
	PyObject *func;
	PyObject *arg;
	PyObject *res;

	if (value == NULL) 
	{
		if (delitemstr == NULL)
		{
			delitemstr = PyString_InternFromString("__delitem__");
		}
		func = instance_getattr(inst, delitemstr);
	}
	else 
	{
		if (setitemstr == NULL)
		{
			setitemstr = PyString_InternFromString("__setitem__");
		}
		func = instance_getattr(inst, setitemstr);
	}
	if (func == NULL)
	{
		return -1;
	}
	if (value == NULL)
	{
		arg = Py_BuildValue("(O)", key);
	}
	else
	{
		arg = Py_BuildValue("(OO)", key, value);
	}
	if (arg == NULL) 
	{
		Py_DECREF(func);
		return -1;
	}
	res = PyEval_CallObject(func, arg);
	Py_DECREF(func);
	Py_DECREF(arg);
	if (res == NULL)
	{
		return -1;
	}
	Py_DECREF(res);
	return 0;
}

static PyMappingMethods instance_as_mapping = {
	(inquiry)instance_length,		
	(binaryfunc)instance_subscript,		
	(objobjargproc)instance_ass_subscript,	
};

static PyObject *instance_item(PyInstanceObject *inst, int i)
{
	PyObject *func, *arg, *res;

	if (getitemstr == NULL)
	{
		getitemstr = PyString_InternFromString("__getitem__");
	}
	func = instance_getattr(inst, getitemstr);
	if (func == NULL)
	{
		return NULL;
	}
	arg = Py_BuildValue("(i)", i);
	if (arg == NULL) 
	{
		Py_DECREF(func);
		return NULL;
	}
	res = PyEval_CallObject(func, arg);
	Py_DECREF(func);
	Py_DECREF(arg);
	return res;
}

static PyObject *sliceobj_from_intint(int i, int j)
{
	PyObject *start, *end, *res;

	start = PyInt_FromLong((long)i);
	if (!start)
	{
		return NULL;
	}

	end = PyInt_FromLong((long)j);
	if (!end) 
	{
		Py_DECREF(start);
		return NULL;
	}
	res = PySlice_New(start, end, NULL);
	Py_DECREF(start);
	Py_DECREF(end);
	return res;
}


static PyObject *instance_slice(PyInstanceObject *inst, int i, int j)
{
	PyObject *func, *arg, *res;
	static PyObject *getslicestr;

	if (getslicestr == NULL)
	{
		getslicestr = PyString_InternFromString("__getslice__");
	}
	func = instance_getattr(inst, getslicestr);

	if (func == NULL) 
	{
		PyErr_Clear();

		if (getitemstr == NULL)
		{
			getitemstr = PyString_InternFromString("__getitem__");
		}
		func = instance_getattr(inst, getitemstr);
		if (func == NULL)
		{
			return NULL;
		}
		arg = Py_BuildValue("(N)", sliceobj_from_intint(i, j));
	} 
	else 
	{
		arg = Py_BuildValue("(ii)", i, j);
	}

	if (arg == NULL) 
	{
		Py_DECREF(func);
		return NULL;
	}
	res = PyEval_CallObject(func, arg);
	Py_DECREF(func);
	Py_DECREF(arg);
	return res;
}

static int instance_ass_item(PyInstanceObject *inst, int i, PyObject *item)
{
	PyObject *func, *arg, *res;

	if (item == NULL) 
	{
		if (delitemstr == NULL)
		{
			delitemstr = PyString_InternFromString("__delitem__");
		}
		func = instance_getattr(inst, delitemstr);
	}
	else 
	{
		if (setitemstr == NULL)
		{
			setitemstr = PyString_InternFromString("__setitem__");
		}
		func = instance_getattr(inst, setitemstr);
	}
	if (func == NULL)
	{
		return -1;
	}
	if (item == NULL)
	{
		arg = Py_BuildValue("i", i);
	}
	else
	{
		arg = Py_BuildValue("(iO)", i, item);
	}
	if (arg == NULL) 
	{
		Py_DECREF(func);
		return -1;
	}
	res = PyEval_CallObject(func, arg);
	Py_DECREF(func);
	Py_DECREF(arg);
	if (res == NULL)
	{
		return -1;
	}
	Py_DECREF(res);
	return 0;
}

static int instance_ass_slice(PyInstanceObject *inst, int i, int j, PyObject *value)
{
	PyObject *func, *arg, *res;
	static PyObject *setslicestr, *delslicestr;

	if (value == NULL) 
	{
		if (delslicestr == NULL)
		{
			delslicestr =
				PyString_InternFromString("__delslice__");
		}
		func = instance_getattr(inst, delslicestr);
		if (func == NULL) 
		{
			PyErr_Clear();
			if (delitemstr == NULL)
			{
				delitemstr =
				    PyString_InternFromString("__delitem__");
			}
			func = instance_getattr(inst, delitemstr);
			if (func == NULL)
			{
				return -1;
			}

			arg = Py_BuildValue("(N)",
					    sliceobj_from_intint(i, j));
		} 
		else
		{
			arg = Py_BuildValue("(ii)", i, j);
		}
	}
	else 
	{
		if (setslicestr == NULL)
		{
			setslicestr =
				PyString_InternFromString("__setslice__");
		}
		func = instance_getattr(inst, setslicestr);
		if (func == NULL) 
		{
			PyErr_Clear();
			if (setitemstr == NULL)
			{
				setitemstr =
				    PyString_InternFromString("__setitem__");
			}
			func = instance_getattr(inst, setitemstr);
			if (func == NULL)
			{
				return -1;
			}

			arg = Py_BuildValue("(NO)",
					    sliceobj_from_intint(i, j), value);
		} 
		else
		{
			arg = Py_BuildValue("(iiO)", i, j, value);
		}
	}
	if (arg == NULL) 
	{
		Py_DECREF(func);
		return -1;
	}
	res = PyEval_CallObject(func, arg);
	Py_DECREF(func);
	Py_DECREF(arg);
	if (res == NULL)
	{
		return -1;
	}
	Py_DECREF(res);
	return 0;
}

static int instance_contains(PyInstanceObject *inst, PyObject *member)
{
	static PyObject *__contains__;
	PyObject *func;

	if (__contains__ == NULL) 
	{
		__contains__ = PyString_InternFromString("__contains__");
		if (__contains__ == NULL)
		{
			return -1;
		}
	}
	func = instance_getattr(inst, __contains__);
	if (func) 
	{
		PyObject *res;
		int ret;
		PyObject *arg = Py_BuildValue("(O)", member);
		if (arg == NULL) 
		{
			Py_DECREF(func);
			return -1;
		}
		res = PyEval_CallObject(func, arg);
		Py_DECREF(func);
		Py_DECREF(arg);
		if (res == NULL) 
		{
			return -1;
		}
		ret = PyObject_IsTrue(res);
		Py_DECREF(res);
		return ret;
	}

	if (PyErr_ExceptionMatches(PyExc_AttributeError)) 
	{
		PyErr_Clear();
		return _PySequence_IterSearch((PyObject *)inst, member,
					      PY_ITERSEARCH_CONTAINS);
	}
	else
	{
		return -1;
	}
}

static PySequenceMethods instance_as_sequence = {
	(inquiry)instance_length,		
	0,					
	0,					
	(intargfunc)instance_item,		
	(intintargfunc)instance_slice,		
	(intobjargproc)instance_ass_item,	
	(intintobjargproc)instance_ass_slice,	
	(objobjproc)instance_contains,	
};

static PyObject *generic_unary_op(PyInstanceObject *self, PyObject *methodname)
{
	PyObject *func, *res;

	if ((func = instance_getattr(self, methodname)) == NULL)
	{
		return NULL;
	}
	res = PyEval_CallObject(func, (PyObject *)NULL);
	Py_DECREF(func);
	return res;
}

static PyObject *generic_binary_op(PyObject *v, PyObject *w, char *opname)
{
	PyObject *result;
	PyObject *args;
	PyObject *func = PyObject_GetAttrString(v, opname);
	if (func == NULL) 
	{
		if (!PyErr_ExceptionMatches(PyExc_AttributeError))
		{
			return NULL;
		}
		PyErr_Clear();
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}
	args = Py_BuildValue("(O)", w);
	if (args == NULL) 
	{
		Py_DECREF(func);
		return NULL;
	}
	result = PyEval_CallObject(func, args);
	Py_DECREF(args);
	Py_DECREF(func);
	return result;
}


static PyObject *coerce_obj;

static PyObject *half_binop(PyObject *v, PyObject *w, char *opname, binaryfunc thisfunc, 
		int swapped)
{
	PyObject *args;
	PyObject *coercefunc;
	PyObject *coerced = NULL;
	PyObject *v1;
	PyObject *result;
	
	if (!PyInstance_Check(v)) 
	{
		Py_INCREF(Py_NotImplemented);
		return Py_NotImplemented;
	}

	if (coerce_obj == NULL) 
	{
		coerce_obj = PyString_InternFromString("__coerce__");
		if (coerce_obj == NULL)
		{
			return NULL;
		}
	}
	coercefunc = PyObject_GetAttr(v, coerce_obj);
	if (coercefunc == NULL) 
	{
		PyErr_Clear();
		return generic_binary_op(v, w, opname);
	}

	args = Py_BuildValue("(O)", w);
	if (args == NULL) 
	{
		return NULL;
	}
	coerced = PyEval_CallObject(coercefunc, args);
	Py_DECREF(args);
	Py_DECREF(coercefunc);
	if (coerced == NULL) 
	{
		return NULL;
	}
	if (coerced == Py_None || coerced == Py_NotImplemented) 
	{
		Py_DECREF(coerced);
		return generic_binary_op(v, w, opname);
	}
	if (!PyTuple_Check(coerced) || PyTuple_Size(coerced) != 2) 
	{
		Py_DECREF(coerced);
		PyErr_SetString(PyExc_TypeError,
				"coercion should return None or 2-tuple");
		return NULL;
	}
	v1 = PyTuple_GetItem(coerced, 0);
	w = PyTuple_GetItem(coerced, 1);
	if (v1->ob_type == v->ob_type && PyInstance_Check(v)) 
	{
		result = generic_binary_op(v1, w, opname);
	} 
	else 
	{
		if (swapped)
		{
			result = (thisfunc)(w, v1);
		}
		else
		{
			result = (thisfunc)(v1, w);
		}
	}
	Py_DECREF(coerced);
	return result;
}

static PyObject *do_binop(PyObject *v, PyObject *w, char *opname, char *ropname,
                   binaryfunc thisfunc)
{
	PyObject *result = half_binop(v, w, opname, thisfunc, 0);
	if (result == Py_NotImplemented) 
	{
		Py_DECREF(result);
		result = half_binop(w, v, ropname, thisfunc, 1);
	}
	return result;
}

static PyObject *do_binop_inplace(PyObject *v, PyObject *w, char *iopname, char *opname,
			char *ropname, binaryfunc thisfunc)
{
	PyObject *result = half_binop(v, w, iopname, thisfunc, 0);
	if (result == Py_NotImplemented) 
	{
		Py_DECREF(result);
		result = do_binop(v, w, opname, ropname, thisfunc);
	}
	return result;
}

static int instance_coerce(PyObject **pv, PyObject **pw)
{
	PyObject *v = *pv;
	PyObject *w = *pw;
	PyObject *coercefunc;
	PyObject *args;
	PyObject *coerced;

	if (coerce_obj == NULL) 
	{
		coerce_obj = PyString_InternFromString("__coerce__");
		if (coerce_obj == NULL)
		{
			return -1;
		}
	}
	coercefunc = PyObject_GetAttr(v, coerce_obj);
	if (coercefunc == NULL) 
	{
		PyErr_Clear();
		return 1;
	}
	args = Py_BuildValue("(O)", w);
	if (args == NULL) 
	{
		return -1;
	}
	coerced = PyEval_CallObject(coercefunc, args);
	Py_DECREF(args);
	Py_DECREF(coercefunc);
	if (coerced == NULL) 
	{
		return -1;
	}
	if (coerced == Py_None || coerced == Py_NotImplemented) 
	{
		Py_DECREF(coerced);
		return 1;
	}
	if (!PyTuple_Check(coerced) || PyTuple_Size(coerced) != 2) 
	{
		Py_DECREF(coerced);
		PyErr_SetString(PyExc_TypeError,
			   "coercion should return None or 2-tuple");
		return -1;
	}
	*pv = PyTuple_GetItem(coerced, 0);
	*pw = PyTuple_GetItem(coerced, 1);
	Py_INCREF(*pv);
	Py_INCREF(*pw);
	Py_DECREF(coerced);
	return 0;
}

#define UNARY(funcname, methodname) \
static PyObject *funcname(PyInstanceObject *self) { \
	static PyObject *o; \
	if (o == NULL) o = PyString_InternFromString(methodname); \
	return generic_unary_op(self, o); \
}

#define BINARY(f, m, n) \
static PyObject *f(PyObject *v, PyObject *w) { \
	return do_binop(v, w, "__" m "__", "__r" m "__", n); \
}

#define BINARY_INPLACE(f, m, n) \
static PyObject *f(PyObject *v, PyObject *w) { \
	return do_binop_inplace(v, w, "__i" m "__", "__" m "__", \
			"__r" m "__", n); \
}

UNARY(instance_neg, "__neg__")
UNARY(instance_pos, "__pos__")
UNARY(instance_abs, "__abs__")

BINARY(instance_or, "or", PyNumber_Or)
BINARY(instance_and, "and", PyNumber_And)
BINARY(instance_xor, "xor", PyNumber_Xor)
BINARY(instance_lshift, "lshift", PyNumber_Lshift)
BINARY(instance_rshift, "rshift", PyNumber_Rshift)
BINARY(instance_add, "add", PyNumber_Add)
BINARY(instance_sub, "sub", PyNumber_Subtract)
BINARY(instance_mul, "mul", PyNumber_Multiply)
BINARY(instance_div, "div", PyNumber_Divide)
BINARY(instance_mod, "mod", PyNumber_Remainder)
BINARY(instance_divmod, "divmod", PyNumber_Divmod)
BINARY(instance_floordiv, "floordiv", PyNumber_FloorDivide)
BINARY(instance_truediv, "truediv", PyNumber_TrueDivide)

BINARY_INPLACE(instance_ior, "or", PyNumber_InPlaceOr)
BINARY_INPLACE(instance_ixor, "xor", PyNumber_InPlaceXor)
BINARY_INPLACE(instance_iand, "and", PyNumber_InPlaceAnd)
BINARY_INPLACE(instance_ilshift, "lshift", PyNumber_InPlaceLshift)
BINARY_INPLACE(instance_irshift, "rshift", PyNumber_InPlaceRshift)
BINARY_INPLACE(instance_iadd, "add", PyNumber_InPlaceAdd)
BINARY_INPLACE(instance_isub, "sub", PyNumber_InPlaceSubtract)
BINARY_INPLACE(instance_imul, "mul", PyNumber_InPlaceMultiply)
BINARY_INPLACE(instance_idiv, "div", PyNumber_InPlaceDivide)
BINARY_INPLACE(instance_imod, "mod", PyNumber_InPlaceRemainder)
BINARY_INPLACE(instance_ifloordiv, "floordiv", PyNumber_InPlaceFloorDivide)
BINARY_INPLACE(instance_itruediv, "truediv", PyNumber_InPlaceTrueDivide)

static int half_cmp(PyObject *v, PyObject *w)
{
	static PyObject *cmp_obj;
	PyObject *args;
	PyObject *cmp_func;
	PyObject *result;
	long l;

	assert(PyInstance_Check(v));

	if (cmp_obj == NULL) 
	{
		cmp_obj = PyString_InternFromString("__cmp__");
		if (cmp_obj == NULL)
		{
			return -2;
		}
	}

	cmp_func = PyObject_GetAttr(v, cmp_obj);
	if (cmp_func == NULL) 
	{
		PyErr_Clear();
		return 2;
	}

	args = Py_BuildValue("(O)", w);
	if (args == NULL)
	{
		return -2;
	}

	result = PyEval_CallObject(cmp_func, args);
	Py_DECREF(args);
	Py_DECREF(cmp_func);

	if (result == NULL)
	{
		return -2;
	}

	if (result == Py_NotImplemented) 
	{
		Py_DECREF(result);
		return 2;
	}

	l = PyInt_AsLong(result);
	Py_DECREF(result);
	if (l == -1 && PyErr_Occurred()) 
	{
		PyErr_SetString(PyExc_TypeError,
			     "comparison did not return an int");
		return -2;
	}

	return l < 0 ? -1 : l > 0 ? 1 : 0;
}

static int instance_compare(PyObject *v, PyObject *w)
{
	int c;

	c = PyNumber_CoerceEx(&v, &w);
	if (c < 0)
	{
		return -2;
	}
	if (c == 0) 
	{
		if (!PyInstance_Check(v) && !PyInstance_Check(w)) 
		{
			c = PyObject_Compare(v, w);
			Py_DECREF(v);
			Py_DECREF(w);
			if (PyErr_Occurred())
			{
				return -2;
			}
			return c < 0 ? -1 : c > 0 ? 1 : 0;
		}
	}
	else 
	{
		Py_INCREF(v);
		Py_INCREF(w);
	}

	if (PyInstance_Check(v)) 
	{
		c = half_cmp(v, w);
		if (c <= 1) 
		{
			Py_DECREF(v);
			Py_DECREF(w);
			return c;
		}
	}
	if (PyInstance_Check(w)) 
	{
		c = half_cmp(w, v);
		if (c <= 1) 
		{
			Py_DECREF(v);
			Py_DECREF(w);
			if (c >= -1)
			{
				c = -c;
			}
			return c;
		}
	}
	Py_DECREF(v);
	Py_DECREF(w);
	return 2;
}

static int instance_nonzero(PyInstanceObject *self)
{
	PyObject *func, *res;
	long outcome;
	static PyObject *nonzerostr;

	if (nonzerostr == NULL)
	{
		nonzerostr = PyString_InternFromString("__nonzero__");
	}
	if ((func = instance_getattr(self, nonzerostr)) == NULL) 
	{
		PyErr_Clear();
		if (lenstr == NULL)
		{
			lenstr = PyString_InternFromString("__len__");
		}
		if ((func = instance_getattr(self, lenstr)) == NULL) 
		{
			PyErr_Clear();
			return 1;
		}
	}
	res = PyEval_CallObject(func, (PyObject *)NULL);
	Py_DECREF(func);
	if (res == NULL)
	{
		return -1;
	}
	if (!PyInt_Check(res)) 
	{
		Py_DECREF(res);
		PyErr_SetString(PyExc_TypeError,
				"__nonzero__ should return an int");
		return -1;
	}
	outcome = PyInt_AsLong(res);
	Py_DECREF(res);
	if (outcome < 0) 
	{
		PyErr_SetString(PyExc_ValueError,
				"__nonzero__ should return >= 0");
		return -1;
	}
	return outcome > 0;
}

UNARY(instance_invert, "__invert__")
UNARY(instance_int, "__int__")
UNARY(instance_long, "__long__")
UNARY(instance_float, "__float__")
UNARY(instance_oct, "__oct__")
UNARY(instance_hex, "__hex__")

static PyObject *bin_power(PyObject *v, PyObject *w)
{
	return PyNumber_Power(v, w, Py_None);
}

static PyObject *instance_pow(PyObject *v, PyObject *w, PyObject *z)
{	
	if (z == Py_None) 
	{
		return do_binop(v, w, "__pow__", "__rpow__", bin_power);
	}
	else 
	{
		PyObject *func;
		PyObject *args;
		PyObject *result;

		func = PyObject_GetAttrString(v, "__pow__");
		if (func == NULL)
		{
			return NULL;
		}
		args = Py_BuildValue("(OO)", w, z);
		if (args == NULL) 
		{
			Py_DECREF(func);
			return NULL;
		}
		result = PyEval_CallObject(func, args);
		Py_DECREF(func);
		Py_DECREF(args);
		return result;
	}
}

static PyObject *bin_inplace_power(PyObject *v, PyObject *w)
{
	return PyNumber_InPlacePower(v, w, Py_None);
}


static PyObject *instance_ipow(PyObject *v, PyObject *w, PyObject *z)
{
	if (z == Py_None) 
	{
		return do_binop_inplace(v, w, "__ipow__", "__pow__",
			"__rpow__", bin_inplace_power);
	}
	else 
	{
		PyObject *func;
		PyObject *args;
		PyObject *result;

		func = PyObject_GetAttrString(v, "__ipow__");
		if (func == NULL) 
		{
			if (!PyErr_ExceptionMatches(PyExc_AttributeError))
			{
				return NULL;
			}
			PyErr_Clear();
			return instance_pow(v, w, z);
		}
		args = Py_BuildValue("(OO)", w, z);
		if (args == NULL) 
		{
			Py_DECREF(func);
			return NULL;
		}
		result = PyEval_CallObject(func, args);
		Py_DECREF(func);
		Py_DECREF(args);
		return result;
	}
}


#define NAME_OPS 6
static PyObject **name_op = NULL;

static int init_name_op()
{
	int i;
	char *_name_op[] = {
		"__lt__",
		"__le__",
		"__eq__",
		"__ne__",
		"__gt__",
		"__ge__",
	};

	name_op = (PyObject **)malloc(sizeof(PyObject *) * NAME_OPS);
	if (name_op == NULL)
	{
		return -1;
	}
	for (i = 0; i < NAME_OPS; ++i) 
	{
		name_op[i] = PyString_InternFromString(_name_op[i]);
		if (name_op[i] == NULL)
		{
			return -1;
		}
	}
	return 0;
}

static PyObject *half_richcompare(PyObject *v, PyObject *w, int op)
{
	PyObject *method;
	PyObject *args;
	PyObject *res;

	assert(PyInstance_Check(v));

	if (name_op == NULL) 
	{
		if (init_name_op() < 0)
		{
			return NULL;
		}
	}
	if (((PyInstanceObject *)v)->in_class->cl_getattr == NULL) 
	{
		method = instance_getattr2((PyInstanceObject *)v, 
					   name_op[op]);
		if (method == NULL) 
		{
			assert(!PyErr_Occurred());
			res = Py_NotImplemented;
			Py_INCREF(res);
			return res;
		}
	} 
	else 
	{
		method = PyObject_GetAttr(v, name_op[op]);
		if (method == NULL) 
		{
			if (!PyErr_ExceptionMatches(PyExc_AttributeError))
			{
				return NULL;
			}
			PyErr_Clear();
			res = Py_NotImplemented;
			Py_INCREF(res);
			return res;
		}
	}

	args = Py_BuildValue("(O)", w);
	if (args == NULL) 
	{
		Py_DECREF(method);
		return NULL;
	}

	res = PyEval_CallObject(method, args);
	Py_DECREF(args);
	Py_DECREF(method);

	return res;
}

static int swapped_op[] = {Py_GT, Py_GE, Py_EQ, Py_NE, Py_LT, Py_LE};

static PyObject *instance_richcompare(PyObject *v, PyObject *w, int op)
{
	PyObject *res;

	if (PyInstance_Check(v)) 
	{
		res = half_richcompare(v, w, op);
		if (res != Py_NotImplemented)
		{
			return res;
		}
		Py_DECREF(res);
	}

	if (PyInstance_Check(w)) 
	{
		res = half_richcompare(w, v, swapped_op[op]);
		if (res != Py_NotImplemented)
		{
			return res;
		}
		Py_DECREF(res);
	}

	Py_INCREF(Py_NotImplemented);
	return Py_NotImplemented;
}


static PyObject *instance_getiter(PyInstanceObject *self) 
{
	PyObject *func;

	if (iterstr == NULL)
	{
		iterstr = PyString_InternFromString("__iter__");
	}
	if (getitemstr == NULL)
	{
		getitemstr = PyString_InternFromString("__getitem__");
	}

	if ((func = instance_getattr(self, iterstr)) != NULL) 
	{
		PyObject *res = PyEval_CallObject(func, (PyObject *)NULL);
		Py_DECREF(func);
		if (res != NULL && !PyIter_Check(res))
		{
			PyErr_Format(PyExc_TypeError,
				     "__iter__ returned non-iterator "
				     "of type '%.100s'",
				     res->ob_type->tp_name);
			Py_DECREF(res);
			res = NULL;
		}
		return res;
	}
	PyErr_Clear();
	if ((func = instance_getattr(self, getitemstr)) == NULL) 
	{
		PyErr_SetString(PyExc_TypeError, "iteration over non-sequence");
		return NULL;
	}
	Py_DECREF(func);
	return PySeqIter_New((PyObject *)self);
}


static PyObject *instance_iternext(PyInstanceObject *self)
{
	PyObject *func;

	if (nextstr == NULL)
	{
		nextstr = PyString_InternFromString("next");
	}

	if ((func = instance_getattr(self, nextstr)) != NULL) 
	{
		PyObject *res = PyEval_CallObject(func, (PyObject *)NULL);
		Py_DECREF(func);
		if (res != NULL) 
		{
			return res;
		}
		if (PyErr_ExceptionMatches(PyExc_StopIteration)) 
		{
			PyErr_Clear();
			return NULL;
		}
		return NULL;
	}
	PyErr_SetString(PyExc_TypeError, "instance has no next() method");
	return NULL;
}

static PyObject *instance_call(PyObject *func, PyObject *arg, PyObject *kw)
{
	PyThreadState *tstate = PyThreadState_GET();
	PyObject *res, *call = PyObject_GetAttrString(func, "__call__");
	if (call == NULL) 
	{
		PyInstanceObject *inst = (PyInstanceObject*) func;
		PyErr_Clear();
		PyErr_Format(PyExc_AttributeError,
			     "%.200s instance has no __call__ method",
			     PyString_AsString(inst->in_class->cl_name));
		return NULL;
	}
	if (tstate->recursion_depth++ > Py_GetRecursionLimit()) 
	{
		PyErr_SetString(PyExc_RuntimeError,
				"maximum __call__ recursion depth exceeded");
		res = NULL;
	}
	else
	{
		res = PyObject_Call(call, arg, kw);
	}
	tstate->recursion_depth--;
	Py_DECREF(call);
	return res;
}


static PyNumberMethods instance_as_number = {
	(binaryfunc)instance_add,		
	(binaryfunc)instance_sub,		
	(binaryfunc)instance_mul,		
	(binaryfunc)instance_div,		
	(binaryfunc)instance_mod,		
	(binaryfunc)instance_divmod,	
	(ternaryfunc)instance_pow,		
	(unaryfunc)instance_neg,		
	(unaryfunc)instance_pos,		
	(unaryfunc)instance_abs,		
	(inquiry)instance_nonzero,		
	(unaryfunc)instance_invert,		
	(binaryfunc)instance_lshift,	
	(binaryfunc)instance_rshift,	
	(binaryfunc)instance_and,		
	(binaryfunc)instance_xor,		
	(binaryfunc)instance_or,		
	(coercion)instance_coerce,		
	(unaryfunc)instance_int,		
	(unaryfunc)instance_long,		
	(unaryfunc)instance_float,		
	(unaryfunc)instance_oct,		
	(unaryfunc)instance_hex,		
	(binaryfunc)instance_iadd,		
	(binaryfunc)instance_isub,		
	(binaryfunc)instance_imul,		
	(binaryfunc)instance_idiv,		
	(binaryfunc)instance_imod,		
	(ternaryfunc)instance_ipow,		
	(binaryfunc)instance_ilshift,		
	(binaryfunc)instance_irshift,		
	(binaryfunc)instance_iand,		
	(binaryfunc)instance_ixor,		
	(binaryfunc)instance_ior,		
	(binaryfunc)instance_floordiv,		
	(binaryfunc)instance_truediv,		
	(binaryfunc)instance_ifloordiv,		
	(binaryfunc)instance_itruediv,		
};

PyTypeObject PyInstance_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"instance",
	sizeof(PyInstanceObject),
	0,
	(destructor)instance_dealloc,	
	0,					
	0,					
	0,					
	instance_compare,			
	(reprfunc)instance_repr,		
	&instance_as_number,			
	&instance_as_sequence,			
	&instance_as_mapping,			
	(hashfunc)instance_hash,		
	instance_call,		
	(reprfunc)instance_str,			
	(getattrofunc)instance_getattr,		
	(setattrofunc)instance_setattr,		
	0,					
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_CHECKTYPES,
	0,					
	(traverseproc)instance_traverse,	
	0,					
	instance_richcompare,			
 	offsetof(PyInstanceObject, in_weakreflist), 
	(getiterfunc)instance_getiter,		
	(iternextfunc)instance_iternext,	
};

static PyMethodObject *free_list;

PyObject *PyMethod_New(PyObject *func, PyObject *self, PyObject *class_)
{
	PyMethodObject *im;
	if (!PyCallable_Check(func)) 
	{
		PyErr_BadInternalCall();
		return NULL;
	}
	im = free_list;
	if (im != NULL) 
	{
		free_list = (PyMethodObject *)(im->im_self);
		PyObject_INIT(im, &PyMethod_Type);
	}
	else 
	{
		im = PyObject_GC_New(PyMethodObject, &PyMethod_Type);
		if (im == NULL)
		{
			return NULL;
		}
	}
	im->im_weakreflist = NULL;
	Py_INCREF(func);
	im->im_func = func;
	Py_XINCREF(self);
	im->im_self = self;
	Py_XINCREF(class_);
	im->im_class = class_;
	_PyObject_GC_TRACK(im);
	return (PyObject *)im;
}

#define OFF(x) offsetof(PyMethodObject, x)

static PyMemberDef instancemethod_memberlist[] = {
	{"im_class",	T_OBJECT,	OFF(im_class),	READONLY|RESTRICTED,
	 "the class associated with a method"},
	{"im_func",	T_OBJECT,	OFF(im_func),	READONLY|RESTRICTED,
	 "the function (or other callable) implementing a method"},
	{"im_self",	T_OBJECT,	OFF(im_self),	READONLY|RESTRICTED,
	 "the instance to which a method is bound; None for unbound methods"},
	{NULL}
};

static PyObject *instancemethod_getattro(PyObject *obj, PyObject *name)
{
	PyMethodObject *im = (PyMethodObject *)obj;
	PyTypeObject *tp = obj->ob_type;
	PyObject *descr = NULL, *res;
	descrgetfunc f = NULL;

	if (PyType_HasFeature(tp, Py_TPFLAGS_HAVE_CLASS)) 
	{
		if (tp->tp_dict == NULL) 
		{
			if (PyType_Ready(tp) < 0)
			{
				return NULL;
			}
		}
		descr = _PyType_Lookup(tp, name);
	}

	f = NULL;
	if (descr != NULL) 
	{
		f = TP_DESCR_GET(descr->ob_type);
		if (f != NULL && PyDescr_IsData(descr))
		{
			return f(descr, obj, (PyObject *)obj->ob_type);
		}
	}

	res = PyObject_GetAttr(im->im_func, name);
	if (res != NULL || !PyErr_ExceptionMatches(PyExc_AttributeError))
	{
		return res;
	}

	if (f != NULL) 
	{
		PyErr_Clear();
		return f(descr, obj, (PyObject *)obj->ob_type);
	}

	if (descr != NULL) 
	{
		PyErr_Clear();
		Py_INCREF(descr);
		return descr;
	}

	assert(PyErr_Occurred());
	return NULL;
}

static void instancemethod_dealloc(PyMethodObject *im)
{
	_PyObject_GC_UNTRACK(im);
	if (im->im_weakreflist != NULL)
	{
		PyObject_ClearWeakRefs((PyObject *)im);
	}
	Py_DECREF(im->im_func);
	Py_XDECREF(im->im_self);
	Py_XDECREF(im->im_class);
	im->im_self = (PyObject *)free_list;
	free_list = im;
}

static int instancemethod_compare(PyMethodObject *a, PyMethodObject *b)
{
	if (a->im_self != b->im_self)
	{
		return (a->im_self < b->im_self) ? -1 : 1;
	}
	return PyObject_Compare(a->im_func, b->im_func);
}

static PyObject *instancemethod_repr(PyMethodObject *a)
{
	PyObject *self = a->im_self;
	PyObject *func = a->im_func;
	PyObject *klass = a->im_class;
	PyObject *funcname = NULL, *klassname = NULL, *result = NULL;
	char *sfuncname = "?", *sklassname = "?";

	funcname = PyObject_GetAttrString(func, "__name__");
	if (funcname == NULL)
	{
		PyErr_Clear();
	}
	else if (!PyString_Check(funcname)) 
	{
		Py_DECREF(funcname);
		funcname = NULL;
	}
	else
	{
		sfuncname = PyString_AS_STRING(funcname);
	}
	if (klass == NULL)
	{
		klassname = NULL;
	}
	else 
	{
		klassname = PyObject_GetAttrString(klass, "__name__");
		if (klassname == NULL)
		{
			PyErr_Clear();
		}
		else if (!PyString_Check(klassname)) 
		{
			Py_DECREF(klassname);
			klassname = NULL;
		}
		else
		{
			sklassname = PyString_AS_STRING(klassname);
		}
	}
	if (self == NULL)
	{
		result = PyString_FromFormat("<unbound method %s.%s>",
					     sklassname, sfuncname);
	}
	else 
	{
		PyObject *selfrepr = PyObject_Repr(self);
		if (selfrepr == NULL)
		{
			goto fail;
		}
		if (!PyString_Check(selfrepr)) 
		{
			Py_DECREF(selfrepr);
			goto fail;
		}
		result = PyString_FromFormat("<bound method %s.%s of %s>",
					     sklassname, sfuncname,
					     PyString_AS_STRING(selfrepr));
		Py_DECREF(selfrepr);
	}
fail:
	Py_XDECREF(funcname);
	Py_XDECREF(klassname);
	return result;
}

static long instancemethod_hash(PyMethodObject *a)
{
	long x, y;
	if (a->im_self == NULL)
	{
		x = PyObject_Hash(Py_None);
	}
	else
	{
		x = PyObject_Hash(a->im_self);
	}
	if (x == -1)
	{
		return -1;
	}
	y = PyObject_Hash(a->im_func);
	if (y == -1)
	{
		return -1;
	}
	return x ^ y;
}

static int instancemethod_traverse(PyMethodObject *im, visitproc visit, void *arg)
{
	int err;
	if (im->im_func) 
	{
		err = visit(im->im_func, arg);
		if (err)
		{
			return err;
		}
	}
	if (im->im_self) 
	{
		err = visit(im->im_self, arg);
		if (err)
		{
			return err;
		}
	}
	if (im->im_class) 
	{
		err = visit(im->im_class, arg);
		if (err)
		{
			return err;
		}
	}
	return 0;
}

static char *getclassname(PyObject *class_)
{
	PyObject *name;

	if (class_ == NULL)
	{
		name = NULL;
	}
	else
	{
		name = PyObject_GetAttrString(class_, "__name__");
	}
	if (name == NULL) 
	{
		PyErr_Clear();
		return "?";
	}
	if (!PyString_Check(name)) 
	{
		Py_DECREF(name);
		return "?";
	}
	PyString_InternInPlace(&name);
	Py_DECREF(name);
	return PyString_AS_STRING(name);
}

static char *getinstclassname(PyObject *inst)
{
	PyObject *class_;
	char *name;

	if (inst == NULL)
	{
		return "nothing";
	}

	class_ = PyObject_GetAttrString(inst, "__class__");
	if (class_ == NULL) 
	{
		PyErr_Clear();
		class_ = (PyObject *)(inst->ob_type);
		Py_INCREF(class_);
	}
	name = getclassname(class_);
	Py_XDECREF(class_);
	return name;
}

static PyObject *instancemethod_call(PyObject *func, PyObject *arg, PyObject *kw)
{
	PyObject *self = PyMethod_GET_SELF(func);
	PyObject *class_ = PyMethod_GET_CLASS(func);
	PyObject *result;

	func = PyMethod_GET_FUNCTION(func);
	if (self == NULL) 
	{
		int ok;
		if (PyTuple_Size(arg) >= 1)
		{
			self = PyTuple_GET_ITEM(arg, 0);
		}
		if (self == NULL)
		{
			ok = 0;
		}
		else 
		{
			ok = PyObject_IsInstance(self, class_);
			if (ok < 0)
			{
				return NULL;
			}
		}
		if (!ok) 
		{
			PyErr_Format(PyExc_TypeError,
				     "unbound method %s%s must be called with "
				     "%s instance as first argument "
				     "(got %s%s instead)",
				     PyEval_GetFuncName(func),
				     PyEval_GetFuncDesc(func),
				     getclassname(class_),
				     getinstclassname(self),
				     self == NULL ? "" : " instance");
			return NULL;
		}
		Py_INCREF(arg);
	}
	else 
	{
		int argcount = PyTuple_Size(arg);
		PyObject *newarg = PyTuple_New(argcount + 1);
		int i;
		if (newarg == NULL)
		{
			return NULL;
		}
		Py_INCREF(self);
		PyTuple_SET_ITEM(newarg, 0, self);
		for (i = 0; i < argcount; i++) 
		{
			PyObject *v = PyTuple_GET_ITEM(arg, i);
			Py_XINCREF(v);
			PyTuple_SET_ITEM(newarg, i+1, v);
		}
		arg = newarg;
	}
	result = PyObject_Call((PyObject *)func, arg, kw);
	Py_DECREF(arg);
	return result;
}

static PyObject *instancemethod_descr_get(PyObject *meth, PyObject *obj, PyObject *class_)
{
	if (PyMethod_GET_SELF(meth) != NULL ||
	    (PyMethod_GET_CLASS(meth) != NULL &&
	     !PyObject_IsSubclass(class_,  PyMethod_GET_CLASS(meth)))) 
	{
		Py_INCREF(meth);
		return meth;
	}
	if (obj == Py_None)
	{
		obj = NULL;
	}
	return PyMethod_New(PyMethod_GET_FUNCTION(meth), obj, class_);
}

PyTypeObject PyMethod_Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"instance method",
	sizeof(PyMethodObject),
	0,
	(destructor)instancemethod_dealloc,	
	0,					
	0,					
	0,					
	(cmpfunc)instancemethod_compare,
	(reprfunc)instancemethod_repr,	
	0,					
	0,					
	0,					
	(hashfunc)instancemethod_hash,		
	instancemethod_call,			
	0,					
	(getattrofunc)instancemethod_getattro,	
	PyObject_GenericSetAttr,
	0,					
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
	0,					
	(traverseproc)instancemethod_traverse,
	0,					
	0,					
 	offsetof(PyMethodObject, im_weakreflist), 
	0,					
	0,					
	0,					
	instancemethod_memberlist,
	0,					
	0,					
	0,					
	instancemethod_descr_get,
	0,					
	0,				
};

void PyMethod_Fini()
{
	while (free_list) 
	{
		PyMethodObject *im = free_list;
		free_list = (PyMethodObject *)(im->im_self);
		PyObject_GC_Del(im);
	}
}
