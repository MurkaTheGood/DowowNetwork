# Dowow Networking C++
> *Dowow Networking C++* is the C++ library for non-blocking (as well as blocking) interaction between microservices or something that resembles them. Initially created to make interaction between VKEO Telegram Bot and VKEO Master Server more abstract and simple. Nevertheless, it can be used in other projects.

## The main parts of the library
The library consists of:
- `Connection` - a connection between two sockets (may they be TCP or UNIX) that has methods to send and receive data.
- `Server` - a facility that handles the acception of new clients.
- `Client` - a facility that handles the connection to the server.
- `Action` - a data structure that describes an intention to do something (for example, delete a user, send the operation result).
- `Datum` - a data structure that describes the unit of data: a request argument, a response field, etc...
- `Value` - base class for all value types, which are:
    * *00* `ValueUndefined` - an unknown value type
    * *01* `Value64S` - int64\_t
    * *02* `Value64U` - uint64\_t
    * *03* `Value32S` - int32\_t
    * *04* `Value32U` - uint32\_t
    * *05* `Value16S` - int16\_t
    * *06* `Value16U` - uint16\_t
    * *07* `Value8S` - int8\_t
    * *08* `Value8U` - uint8\_t
    * *09* `ValueStr` - the sequence of bytes of known length which contains the text
    * *0A* `ValueArr` - an array of values of any types

## The main points of `Connection`?
- `Connection` objects are used to perform data transfer between two sockets.
- `Connection` objects *do not* permit raw data transfer.
- `Connection` objects implement the sending and receival of `Actions`s.
- `Connection` objects implement the non-blocking operations as well as blocking ones.

## The main points of `Server`?
- `Server` objects are used to accept connections from clients.
- `Server` objects return `Connection` pointers in their accept function.
- `Server` objects implement the non-blocking operation as well as blocking ones.

## The main points of `Client`?
- `Client` objects are used to connect to the server.
- `Client` objects are implemented as the child class of `Connection`.

## The main points of `Action`?
- `Action` objects are used to describe the action, that is to be done.
- `Action` objects have IDs, which are unique for a single Connection.
- `Action` objects have names, which are used to name the needed operation. It works like function name.
- `Action` objects have `Datum` objects, which are used to specify the data that is needed to perform the action.
- `Datum` objects owned by `Action` objects have unique names, and may be indexed only by names.

## The main points of `Datum`?
- `Datum` objects are named fields of `Action` and `Reaction` objects.
- `Datum` objects have names (only ASCII).
- `Datum` objects have value

## The main points of `Value` derived classes?
- `Value` objects have type
- `Value` objects store the data
- `Value` objects convert the data to raw bytes
- `Value` objects convert the raw bytes to data

## Serialized structure of Value
> The length of the serialized Value must be at least 5 bytes.
*The Value header:*
0 - 1: the type code stored as uint8\_t
1 - 5: the length of the value stored as little-endian uint32\_t
*The Value content*:
- `Value__N` *where \_\_ is bit count, N is Signed/Unsigned*: stored as little-endian integers of corresponding size
- `ValueStr`: first 4 bytes are the length stored as the little-endian uint32\_t, the rest is the string of specified length without trailing zero
- `ValueArr`: first 4 bytes are the amount of elements N stored as the little-endian uint32\_t, the rest is stream of N values

## Serialized structure of Datum
> The length of the serialized Datum must be at least 6 bytes.
*The structure is:*
0 - 4: the length L of the Datum (including the metadata) stored as little-endian uint32\_t
4 - 6: the length N of Datum name (no trailing zero) stored as little-endian uint16\_t
6 - 6+N: the name of the datum (ASCII string without trailing zero)
6+N - ...: the serialized value

## Serialized structure of Action
> The length of the serialized Action must be at least 10 bytes.
*The structure is:*
0 - 4: the length L of the Action (including the metadata) stored as little-endian uint32\_t
4 - 8: the Action ID stored as little-endian uint32\_t
8 - 10: the length N of the Action name (no trailing zero) stored as little-endian uint16\_t
10 - 10+N: the name of the Action (ASCII string without trailing zero)
10+N - ...: the stream of Datums
