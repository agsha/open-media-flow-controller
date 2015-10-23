#include <python2.6/Python.h>
#include "ssp_python_plugin.h"
#include "nkn_errno.h"
#include "nkn_debug.h"

/*
 *
 *  embedding python in multithreading C program  need the python GIL
 *
 */

/*
 * BZ 8382
 * Transcode script call
 * Due to Python GLI
 * The C2Python interface for transcode is not used anymore in multi-thread
 *
 * VPE Publishing Python Interpretor
 */

#if 0
int pyVpePublish(const char 	*hostData,
		 int 		hostLen,
		 const char 	*uriData,
		 int 		uriLen,
		 const char 	*stateQuery,
		 int 		stateQueryLen)
{
    PyObject *pName, *pModule, *pDict, *pFunc;
    PyObject *pArgs, *pValue;
    int val=10, i;
    int rv=0;

    char pyModule[] = "pyVpePublish";
    char pyFunction[] = "publish_init";

    // Initialize the Python Interpreter
    Py_Initialize();

    pModule = PyImport_ImportModule(pyModule);

    if (pModule != NULL) {
	pFunc = PyObject_GetAttrString(pModule, pyFunction);

	if (pFunc && PyCallable_Check(pFunc)){
	    pArgs = Py_BuildValue("(sisisi)", hostData, hostLen, uriData, uriLen, stateQuery, stateQueryLen);

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

    return rv;
}
#endif /* 0 */


/*
 * BZ 8382
 * Transcode script call
 * Due to Python GLI
 * The C2Python interface for transcode is not used anymore in multi-thread
 *
 *  VPE Transcode Python Interpretor
 */

#if 0
int pyVpeGenericTranscode(const char 	*hostData,
		 int 		hostLen,
		 const char 	*uriData,
		 int 		uriLen,
		 int 		transRatio,
		 int            transComplex)
{
    PyObject *pName, *pModule, *pDict, *pFunc;
    PyObject *pArgs, *pValue;
    int val=10, i;
    int rv=0;

    char pyModule[] = "pyGenericTranscode";
    char pyFunction[] = "genericSSPTranscode";

    // Initialize the Python Interpreter
    Py_Initialize();

    pModule = PyImport_ImportModule(pyModule);

    if (pModule != NULL) {
	pFunc = PyObject_GetAttrString(pModule, pyFunction);

	if (pFunc && PyCallable_Check(pFunc)){
	  pArgs = Py_BuildValue("(sisiii)", hostData, hostLen, uriData, uriLen, transRatio,transComplex);
	  //  pArgs = Py_BuildValue("(sisisi)", hostData, hostLen, uriData, uriLen, stateQuery, stateQueryLen);
	      // pArgs = Py_BuildValue("(sisisi)", hostData, hostLen, uriData, uriLen, "stateQuery",10 );
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

    return rv;
}

#endif /* 0 */

/*
 * BZ 8382
 * Transcode script call
 * Due to Python GLI
 * The C2Python interface for transcode is not used anymore in multi-thread
 */

#if 0
int pyVpeYoutubeTranscode(const char 	*hostData,
			  int 		hostLen,
			  const char 	*uriData,
			  int 		uriLen,
			  const char   *queryName,
			  int           queryLen,
			  const char   *remapUriData,
			  int           remapUriLen,
			  const char   *nsuriPrefix,
			  int           nsuriPrefixLen,
			  int 		transRatio,
			  int            transComplex)
{
    PyObject *pName, *pModule, *pDict, *pFunc;
    PyObject *pArgs, *pValue;

    PyThreadState *mainThreadState, *myThreadState, *tempState;
    PyInterpreterState *mainInterpreterState;

    int val=10, i;
    int rv=0;

    char pyModule[] = "pyYoutubeTranscode";
    char pyFunction[] = "youtubeSSPTranscode";

    // Initialize the Python Interpreter
    Py_Initialize();

    // Initialize thread support
    PyEval_InitThreads();

    // Save a pointer to the main PyThreadState object
    mainThreadState = PyThreadState_Get();

    // Get a reference to the PyInterpreterState
    mainInterpreterState = mainThreadState->interp;

    // Create a thread state object for this thread
    myThreadState = PyThreadState_New(mainInterpreterState);

    // Release global lock
    PyEval_ReleaseLock();

    // Acquire global lock
    PyEval_AcquireLock();

    // Swap in my thread state
    tempState = PyThreadState_Swap(myThreadState);

    pModule = PyImport_ImportModule(pyModule);

    if (pModule != NULL) {
	pFunc = PyObject_GetAttrString(pModule, pyFunction);

	if (pFunc && PyCallable_Check(pFunc)){
	  pArgs = Py_BuildValue("(sisisisisiii)", hostData, hostLen, uriData, uriLen, queryName,queryLen,
				remapUriData, remapUriLen, nsuriPrefix, nsuriPrefixLen, transRatio, transComplex);
	    fprintf(stdout, "Start Call Python Plugin\n");
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

    // Swap out the current thread
    PyThreadState_Swap(tempState);

    // Release global lock
    PyEval_ReleaseLock();

    // Clean up thread state
    PyThreadState_Clear(myThreadState);
    PyThreadState_Delete(myThreadState);

    // Finish the Python Interpreter
    Py_Finalize();

    return rv;
}
#endif /* 0 */

/*
 * Sample Python call for Break Hash check
 *
 */
int py_checkMD5Hash(const char *hostData, int hostLen, const char *uriData, const char *queryHash)
{
  PyObject *pName, *pModule, *pDict, *pFunc;
  PyObject *pArgs, *pValue;
  int rv=0;

  char perlModule[] = "pyHashCheck";
  char perlFunction[] = "checkMD5";

  // Initialize the Python Interpreter
  Py_Initialize();

  pName = PyString_FromString(perlModule);
  /* Error checking of pName left out */

  pModule = PyImport_Import(pName);
  Py_DECREF(pName);

  if (pModule != NULL) {
    // pDict is a borrowed reference
    pDict = PyModule_GetDict(pModule);

    // pFunc is also a borrowed reference
    pFunc = PyDict_GetItemString(pDict, perlFunction);

    if (pFunc && PyCallable_Check(pFunc)) {
      pValue = PyObject_CallFunction(pFunc, "siss", hostData, hostLen, uriData, queryHash);

      if (pValue != NULL) {
	rv =  PyInt_AsLong(pValue);
        Py_DECREF(pValue);
      }
      else {
        Py_DECREF(pFunc);
        Py_DECREF(pModule);
        PyErr_Print();
	rv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
	DBG_LOG(WARNING, MOD_SSP, "Python Plugin - Call unsucessfull. (Error: %d)", NKN_SSP_PYTHON_PLUGIN_FAILURE);
      }
    }
    else {
      if (PyErr_Occurred())
        PyErr_Print();

      rv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
      DBG_LOG(WARNING, MOD_SSP, "Python Plugin - Cannot find function %s (Error: %d)", perlFunction, NKN_SSP_PYTHON_PLUGIN_FAILURE);
    }
    Py_XDECREF(pFunc);
    Py_DECREF(pModule);
  }
  else // pModule is NULL
    {
      PyErr_Print();

      rv = -NKN_SSP_PYTHON_PLUGIN_FAILURE;
      DBG_LOG(WARNING, MOD_SSP, "Python Plugin - Failed to load perl module %s (Error: %d)", perlModule, NKN_SSP_PYTHON_PLUGIN_FAILURE);
    }

  // Finish the Python Interpreter
  Py_Finalize();

  return rv;
}
