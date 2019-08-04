Ctrl + Z : exit

----------------------------------
__lltrace__ = True
del __lltrace__

A Whirlwind Excursion through Python C Extensions
https://nedbatchelder.com/text/whirlext.html
http://stackoverflow.com/questions/1728472/i-want-to-start-reading-the-python-source-code-where-should-i-start

https://svn.python.org/projects/python/trunk/

http://akaptur.com/blog/2013/11/15/introduction-to-the-python-interpreter/
http://developer.51cto.com/art/201401/428125.htm
https://github.com/akaptur/akaptur.github.com

----------------------------------
python/exceptions.c
	_PyExc_Init() line 990
	Py_Initialize() line 170
	Py_Main(int 1, char * * 0x005f0ec0) line 325
http://tieba.baidu.com/p/3192220799
>>> import exceptions
>>> dir(exceptions)
----------------------------------


todo:
(x, only #if) 1. remove macro #define
(x) 2. remove register
(x) 3. ramove param void
(x)(-) 4. rename class->class_
(?) 5. //no break
(x) 6. rename new->new_
(x) 7. \-> "xxx"
(x) 8. remove (void) : (void)xxx(...); int xxx(void);
(x) 9. \n\ -> "...\n"
(x) 10. remove \
(x) 11. #if 0
(x) 12. remove (void)xxx() -> xxx()
(?) 13. embeded struct  
----------------------------------


stringobject.c:3032
unicodeobject.c:947
unicodeobject.c:2183
unicodeobject.c:2683

-------------------------------------------

ceval:2123

--------------------------------------------

import sys <-----cached
import thread <---------initthread

--------------------------------------------
