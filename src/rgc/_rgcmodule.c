#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "rgc.h"

static PyObject* py_rgc_compress(PyObject* self, PyObject* args) {
    const unsigned char* input_data;
    Py_ssize_t input_size;
    uint8_t* compressed_data = NULL;
    int32_t compressed_size = 0;
    PyObject* result;

    (void)self;

    if (!PyArg_ParseTuple(args, "y#", &input_data, &input_size)) {
        return NULL;
    }

    if (input_size > INT32_MAX) {
        PyErr_SetString(PyExc_OverflowError, "input is too large for rgc_encode()");
        return NULL;
    }

    rgc_encode(input_data, (int32_t)input_size, &compressed_data, &compressed_size);
    if (compressed_data == NULL) {
        PyErr_SetString(PyExc_MemoryError, "rgc_encode() failed");
        return NULL;
    }

    result = PyBytes_FromStringAndSize((const char*)compressed_data, compressed_size);
    rgc_free(compressed_data);
    return result;
}

static PyObject* py_rgc_decompress(PyObject* self, PyObject* args) {
    const unsigned char* input_data;
    Py_ssize_t input_size;
    uint8_t* decoded_data = NULL;
    int32_t decoded_size = 0;
    PyObject* result;

    (void)self;

    if (!PyArg_ParseTuple(args, "y#", &input_data, &input_size)) {
        return NULL;
    }

    if (input_size > INT32_MAX) {
        PyErr_SetString(PyExc_OverflowError, "input is too large for rgc_decode()");
        return NULL;
    }

    rgc_decode(input_data, (int32_t)input_size, &decoded_data, &decoded_size);
    if (decoded_data == NULL) {
        PyErr_SetString(PyExc_ValueError, "rgc_decode() failed");
        return NULL;
    }

    result = PyBytes_FromStringAndSize((const char*)decoded_data, decoded_size);
    rgc_free(decoded_data);
    return result;
}

static PyMethodDef rgc_methods[] = {
    {"compress", py_rgc_compress, METH_VARARGS, "Compress a bytes object with RGC."},
    {"decompress", py_rgc_decompress, METH_VARARGS, "Decompress a bytes object with RGC."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef rgc_module = {
    PyModuleDef_HEAD_INIT,
    "_rgc",
    "Python wrapper for Recursive Group Coding.",
    -1,
    rgc_methods
};

PyMODINIT_FUNC PyInit__rgc(void) {
    return PyModule_Create(&rgc_module);
}
