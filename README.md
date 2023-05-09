# Dowow Networking C++
**Dowow Networking C++** is the C++ library for non-blocking (as well as blocking) interaction between microservices or something that resembles them. The library was initially created to make interaction between VKEO Telegram Bot and VKEO Master Server more abstract and simple. It turned out that the library might be useful for other projects.

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

## The main points of `Connection`?
- `Connection` objects are used to perform data transfer between two sockets.
- `Connection` objects **do not** permit raw data transfer. That was intended.
- `Connection` objects can transfer requests.
- `Connection` objects implement the non-blocking operations as well as blocking ones.

## The main points of `Server`?
- `Server` objects are used to accept connections.
- `Server` objects implement the non-blocking operations as well as blocking ones.

## The main points of `Client`?
- `Client` objects are used to connect to the server.
- `Client` objects are implemented as the child class of `Connection`.

## The main points of `Request`?
- `Request` objects have IDs, which are used to exactly identify the request.
- `Request` objects have names, which are used to specify the wanted operation (acts like a function name).
- `Request` objects have `Datum` objects, which are used as arguments.

## The main points of `Datum`?
- `Datum` objects are named fields of `Request` objects.
- `Datum` objects have names (only ASCII).
- `Datum` objects have value

## The main points of `Value` derived classes?
- `Value` objects have type
- `Value` objects store the data
- `Value` objects convert the data to raw bytes (serialize it)
- `Value` objects convert the raw bytes to data (deserialize it)
