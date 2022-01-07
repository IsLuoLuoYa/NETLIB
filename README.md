# NETBASE
使用时必做的是

    #include "BaseControl.h"
    
    int main()
    
    {
    
      FairySunOfNetBaseStart();
      
      //pause();
      
      FairySunOfNetBaseOver();
      
      return 0;
      
    }

一些参数没有暴露出来，比如说第二缓存区的大小(默认16384)，暂时也没有遇到需求，以后可能会改出来
