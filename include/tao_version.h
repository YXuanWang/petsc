#if !defined(__TAO_VERSION_H)
#define __TAO_VERSION_H

/* ========================================================================== */
#define TAO_VERSION_RELEASE  1 /*  1 if official release */
/* 
   Current TAO version number and release date
*/
#define TAO_VERSION_NUMBER "TAO Version 2.2.1"
#define TAO_VERSION_MAJOR    2
#define TAO_VERSION_MINOR    2
#define TAO_VERSION_SUBMINOR 1
#define TAO_VERSION_PATCH    0
#define TAO_VERSION_(MAJOR,MINOR,SUBMINOR) \
    ((TAO_VERSION_MAJOR == (MAJOR)) &&       \
    (TAO_VERSION_MINOR == (MINOR)) &&       \
     (TAO_VERSION_SUBMINOR == (SUBMINOR)))
#define TAO_VERSION_DATE     "Sept 20, 2013"
#define TAO_AUTHOR_INFO      "The TAO Team:\
 Todd Munson, Jason Sarich, Stefan Wild\n\
Bug reports, questions: tao-comments@mcs.anl.gov\n\
Web page: http://www.mcs.anl.gov/tao/\n"

#endif
