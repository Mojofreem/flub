#ifndef _LICENSE_HEADER_
#define _LICENSE_HEADER_


typedef int (*licenseCB_t)(const char *text, void *context);

void licenseEnumComponents(licenseCB_t callback, void *context);
void licenseEnumLicenses(licenseCB_t callback, void *context);
void licenseEnumCopyrights(licenseCB_t callback, void *context);

void licenseGet(licenseCB_t callback, void *context, const char *component);
void licenseGetAll(licenseCB_t callback, void *context);


#endif // _LICENSE_HEADER_
