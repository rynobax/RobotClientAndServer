# ROBOT PROXY CLIENT AND SERVER

----------------------------------------------------------------

## GROUP MEMBERS:

- Ryan Baxley
- REDACTED
- REDACTED
- REDACTED
- REDACTED

------------------------------------------------------------------------
## DESCRIPTION:

The purpose of this project was to write an HTTP client that can 
perform HTTP get requests for 7 data sources on prescribed
hostname/ports. This HTTP client also functions as an 
UDP server on a prescribed port. This port is identified via a 
command line parameter. The UDP server provides UDP clients with 
the result of the appropriate HTTP get request.

We also wrote a UDP client to interact with our HTTP client that saves 
the content that it receives (images and text). The hostname and port 
for the UDP server are defined by command line parameters for this 
client program as well.

The robotClient performs the following tasks:

1. Make a robot traverse the path of a two regular polygons in sequence.

2. The polygons are length L and order N and N-1 (where L and N are 
   integers provided by the user in addition to the other parameters).

3. 4 <= N <= 8.

------------------------------------------------------------------------
DESIGN:
robotClient:
	-The client is based on the UDPEchoClient that was used for the ValueGuesser
	 project.
	-It calls the issueCommands() function twice, once for N and once for N-1
	-The issueCommands() function calls a series functions that send data, 
	 move, and turn requests to the middleWare.
	-Those functions create a UDP connection to the middleware, and sends
	 a UDCP request to the server.
	-It then recieves all the messages from the middleware and saves them
	 to file, checking sequence numbers to ensure that they are in order.

robotServer:
	-The server is based on both the UDPEchoServer used in the ValueGuesser and
	 the simGet client.
	-It runs in an infinite loop, accepting incoming UDP packets.
	-When it recieves a packet, it verifies the robotID and parses out the request
	-It then makes the appropriate HTTP get to the robot.
	-It stores the HTTP response, and uses multiple UDP packets to relay the
	 body of the response to the client.
------------------------------------------------------------------------
KNOWN PROBLEMS:
The server cannot store HTTP responses over 50000 bytes
------------------------------------------------------------------------
