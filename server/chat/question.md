```

  session::start()
  {
    aync-readHead();
  }
  aync-readHead()
  {
    ....
    aync-readBody();
  }
  aync-readBody()
  {
    ... getting normal-package
    service-push(package);
    aync-readHead();
  }

  thread => service-run(){
  [lock-grade]:
    while(packageQueue.empty() == false) 
      callService(packageQueue.pop());  // if call the service is A
  }

  A()
  {
    session-send(...);

    
    // how read?
    aync-readHead();
  }

```