
#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#ifndef MAXSIZE_GROUP
#define MAXSIZE_GROUP 256
#endif

#define MYTIMEVAL( tv_ )			\
  ((tv_.tv_sec)+(tv_.tv_usec)*1.0e-6)

#define TIMESTAMP( time_ )				\
  {							\
    static struct timeval tv;				\
    gettimeofday( &tv, NULL );				\
    time_=MYTIMEVAL(tv);				\
  }

#define GPTR_SPRINTF(buf_, gptr_)				\
  sprintf(buf_, "(unit=%d,seg=%d,flags=%d,addr=%p)",		\
	  gptr_.unitid, gptr_.segid, gptr_.flags,		\
	  gptr_.addr_or_offs.addr);


#define CHECK(fncall) do {					     \
    int _retval;                                                     \
    if ((_retval = fncall) != DART_OK) {                             \
      fprintf(stderr, "ERROR %d calling: %s"                         \
	      " at: %s:%i",					     \
              _retval, #fncall, __FILE__, __LINE__);                 \
      fflush(stderr);                                                \
    }                                                                \
  } while(0)


#define GROUP_SPRINTF(buf_, group_)					\
  do {									\
    size_t size;							\
    char *str;								\
    int i,len;								\
    dart_unit_t members[MAXSIZE_GROUP];					\
    dart_group_size(group_, &size);					\
    dart_group_getmembers(group_, members);				\
    str=buf_;								\
    len=sprintf(str, "size=%d members=", size);				\
    str=str+len;							\
    for( i=0; i<size; i++ ) {						\
      len=sprintf(str, "%d ", members[i]);				\
      str=str+len;							\
    }									\
  }									\
  while(0)



#endif /* UTILS_H_INCLUDED */
