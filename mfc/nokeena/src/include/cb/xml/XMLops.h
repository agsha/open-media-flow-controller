/*
 * XMLops.h -- Generic interface to XML operations
 */
#ifndef _XMLOPS_H_
#define _XMLOPS_H_

typedef struct XMLDocCTX {
  int current_recnum;
  void *XMLdoc;
  void *XMLrdr;
} XMLDocCTX_t;

typedef struct XMLops {
   /* Return: !=0, Success */
   XMLDocCTX_t *(*newXMLDocCTX)(void);

   /* Return: ==0, Success */
   int (*resetXMLDocCTX)(XMLDocCTX_t *ctx);

   void (*deleteXMLDocCTX)(XMLDocCTX_t *ctx);

   /* Return: ==0, Sucess */
   int (*validateXML)(const char *input_file, const char *DTDdata,
		      XMLDocCTX_t *ctx, char *errbuf, int sizeof_errbuf);

   /*
    * Sequentially retrieve the XML elements and translate them to 
    * protocol specific struct definition (protoStruct).
    *
    * Return: ==0, Success < 0, EOF > 0, Error
    */
   int (*XML2Struct)(XMLDocCTX_t *ctx, void *protoStruct, 
		     char *errbuf, int sizeof_errbuf);
} XMLops_t;

#endif /* _XMLOPS_H_ */

/*
 * End of XMLops.h
 */
