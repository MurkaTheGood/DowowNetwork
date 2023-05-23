# Dowow Networking
**Dowow Network** is a multithreaded C++ library for Dowow Protocol. It can be used in both UNIX and TCP/IP domains.

## How to build?
Run the following in your terminal:
```
git clone https://github.com/MurkaTheGood/DowowNetwork/
cd DowowNetwork
mkdir build; cd build
cmake ..
make all
```
And the library will be in file libDowowNetwork.a

## How to build documentation?
In the repository root directory run the following:
```
doxygen
```
And the documentation in HTML and MAN will be generated in directory `docs`

## Quick introduction
First of all it is important to know what Dowow Protocol is.
Dowow Protocol is a protocol of data transmission format, that defines the following entities:
1. Value,
2. Datum (which always contains one Value),
3. Request (which contains zero or more Datums),
4. Connection (which performs data transfer between two endpoints),
5. Client and Server (in their typical meaning).
There are always two endpoints:
1. Client (the side that initiates the connection),
2. Server (the side that accepts the Client's connection invitation).

### Connection:
There is a thread that I call "polling thread" because it handles I/O operation. Then a Connection becomes connected to the remote endpoint that thread is
started and is running while the Connection is alive, i.e. connected. Once disconnection occurs that thread is stopped.
#### Pull():
When the user calls the Pull() method, that's used for receiving the data, it must specify the timeout. If the timeout is nonzero then the call is considered to be
blocking. Blocking call will return once there is data to return, the call is timed out ar an error occurs. If there is data to return then method Pull() returns
the pointer to a Request that was received. If an error occured or the call is timed out, null-pointer is returned. The timeout is specified in seconds. If timeout
is less than zero then the call will never get timed out and will return only in case of failure or success. If the timeout value is more than zero then the timeout
has obvious meaning. If the timeout is zero then the call is nonblocking, i.e. it will return immidiately. A valid Request pointer is returned if there is data. A
null-pointer is returned if there is no data or there is an error. It is important to note that Pull() method does not implement any error-checking mechanism. You
are to call IsConnected() after the call of Pull() to check if it returned null-pointer because of connection problems.
#### Push():
When the user calls the Push() method, that's used for sending the data, the Request is pushed to the queue of requests to be sent. The poller thread will eventually
check the queue and send the Request to the remote endpoint. If you specify the "timeout" parameter of the Push() method then this method can be also used for
response receival. If timeout <= -1, then the method will return only on response receival or disconnection. If timeout is 0, then the method will not wait for
response and will just push the Request to the queue. If timeout > 0, then the method will wait for response for *timeout* seconds; if no response arrives,
null-pointer is returned; if the response arrives, it is returned; if an error occurs, null-pointer is returned.

## The main parts of the library
The library consists of:
- `Connection` - a connection between two sockets (may they be TCP or UNIX) that has methods to send and receive requests (that is, no raw data transfer).
- `Client` - a facility that handles the base client logic. Implemented as a `Connection`'s derived class.
- `Server` - a facility that handles the acception of new clients.
- `Request` - a data structure that describes an intention to do something (for example, delete a user, send the operation result). Each request has ID which is used in response receival.
- `Datum` - a data structure that describes the unit of data: a request argument, a response field...
- `Value` - base class for all value types, which are:
    * `ValueUndefined` - an unknown value type
    * `Value64S` - int64\_t
    * `Value64U` - uint64\_t
    * `Value32S` - int32\_t
    * `Value32U` - uint32\_t
    * `Value16S` - int16\_t
    * `Value16U` - uint16\_t
    * `Value8S` - int8\_t
    * `Value8U` - uint8\_t
    * `ValueStr` - the sequence of bytes of known length which contains the text
    * `ValueArr` - an array of values of any types
