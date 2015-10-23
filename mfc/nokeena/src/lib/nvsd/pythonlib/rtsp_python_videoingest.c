#include <python2.6/Python.h>
#include "ssp_python_plugin.h"
#include "nkn_errno.h"
#include "nkn_debug.h"

/*
 * RTSP Video Offline Ingest 
 *
 *
 */
int pyRtspIngest(const char 	*hostData,
		 int 		hostLen,
		 const char 	*uriData,
		 int 		uriLen)
{
    int rv=0;

    /* Not clear whether to use Python for RTSP ingest as of now.
       Hence maintaining this code as template for future use, in case we decide to use Python for processing */

#if 0
    PyObject *pName, *pModule, *pDict, *pFunc;
    PyObject *pArgs, *pValue;
    int val=10, i;

    char pyModule[] = "pyRtspIngest";
    char pyFunction[] = "init_rtpingest";

    // Initialize the Python Interpreter
    Py_Initialize();

    pModule = PyImport_ImportModule(pyModule);

    if (pModule != NULL) {
	pFunc = PyObject_GetAttrString(pModule, pyFunction);

	if (pFunc && PyCallable_Check(pFunc)){
	    pArgs = Py_BuildValue("(sisi)", hostData, hostLen, uriData, uriLen);

	    pValue = PyObject_CallObject(pFunc, pArgs);
            Py_DECREF(pArgs);

	    if (pValue != NULL) {
		PyArg_Parse(pValue, "i", &rv);
                fprintf(stdout, "Python Plugin - Call Sucessfull. (RV= %d)\n", rv);
                Py_DECREF(pValue);
	    }
	    else {
		Py_DECREF(pFunc);
                Py_DECREF(pModule);
                PyErr_Print();
                rv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
                fprintf(stdout, "Python Plugin - Call unsucessfull. (Error: %d)\n", NKN_SSP_PYTHON_PLUGIN_FAILURE);
		return rv;
	    }
	}
	else {
	    if (PyErr_Occurred())
                PyErr_Print();

            rv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
            fprintf(stdout, "Python Plugin - Cannot find function %s (Error: %d)\n", pyFunction, NKN_SSP_PYTHON_PLUGIN_FAILURE);
	}
	Py_XDECREF(pFunc);
        Py_DECREF(pModule);
    }
    else // pModule is NULL
	{
	    PyErr_Print();
	    rv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
	    fprintf(stdout, "Python Plugin - Failed to load python module %s (Error: %d)", pyModule, NKN_SSP_PYTHON_PLUGIN_FAILURE);
	    return rv;
	}

    // Finish the Python Interpreter
    Py_Finalize();
#endif

    return rv;
}
