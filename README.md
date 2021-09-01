# rabbitmq-cobol
Accompanying code for http://assortedrambles.blogspot.com/2013/04/using-rabbitmq-from-cobol_9584.html.

This repository contains code and examples to accompany this rather old blog post about interacting with RabbitMQ from COBOL. The code was pulled together fairly quickly for the purposes of the blog and accordingly it is not of “production quality”, and should be treated with some degree of caution. The code is not at all well-documented, nor necessarily complete, but is posted here in the hope that it may be useful. That being said, the examples do work, and it would not take too much effort to beat things into better shape!

Some points to note:

  - The COBOL API assumes one channel per connection. Generally speaking there is plenty of scope to expand the API to include more and better functionality.
  - The COBOL examples were created and tested using GnuCOBOL.
  - The makefiles are totally brain-dead.
  - Be sure to read the blog before trying to do anything much with the code (it will make things at least somewhat clearer).
  - The examples have broker connectivity and other details hardwired; these will need to be changed as appropriate to your environment. One or two of the examples also assume some broker configuration details (queues, bindings).

Enjoy!
