#include <Python.h>
#include <stdio.h>
#include <numarray/libnumarray.h>
#include <numarray.h>

void wrappertemplate_printit(float *data, int nx, int ny);

static PyObject *
PyWrapperTemplate_printit(PyObject *self, PyObject *args)
{
  PyObject *dataObject;
  PyArrayObject *dataArray;
  float *data;
  int nx, ny;
  
  if(!PyArg_ParseTuple(args, "O", &dataObject))
    return NULL;

  dataArray = NA_InputArray(dataObject, tFloat32, C_ARRAY);  
  printf("%c\n", dataArray->descr->type);
  printf("%d\n", dataArray->itemsize);
  if(dataArray->nd != 2) {
    return PyErr_Format(PyExc_AttributeError, "Only works on 2D arrays.");
  }
  data = (float *) NA_OFFSETDATA(dataArray);
  nx=dataArray->dimensions[0];
  ny=dataArray->dimensions[1];

  wrappertemplate_printit(data, nx, ny);
  Py_XDECREF(dataArray);
  return (PyObject *)Py_BuildValue("O", dataArray);
}

static PyMethodDef
PyWrapperTemplateMethods[] =
{
  { "printit", PyWrapperTemplate_printit, METH_VARARGS },
  { NULL, NULL },
};

void initPyWrapperTemplate(void)
{
  import_libnumarray();
  Py_InitModule("PyWrapperTemplate", PyWrapperTemplateMethods);
}
