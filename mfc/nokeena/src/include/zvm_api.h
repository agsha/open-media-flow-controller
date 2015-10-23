/*Zvm notification api's, used only in the case of ZVM build */
#if !defined (_ZVM_API_H_)
#define _ZVM_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NOZCC
#define NOZCC(var)
#endif
#define NOZCC_DECL(x) NOZCC(x) (x)


#ifdef __ZVM__
#define ZVM_ENQUEUE(elem) zvm_api_enqueue(NULL, elem)
#define ZVM_DEQUEUE(elem) zvm_api_dequeue(NULL, elem);
#else
#define ZVM_ENQUEUE(elem)
#define ZVM_DEQUEUE(elem)
#endif

#ifdef __cplusplus
}
#endif


#endif /* _ZVM_API_H_ */
