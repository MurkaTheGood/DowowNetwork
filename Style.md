# Code styling agreements

## Acroyms
* If the acronym is used inside snake\_case then it must be written in lower case. Examples:
    - connect\_**tcp**()
    - is\_connected\_**tcp**()
* If the acronym is used inside PascalCase or camelCase then it must be capitalized and written in lower case.
camelCase has an exception: if the name begins with acronym, then it must be written in lower case entirely.
Examples:
    - ConenctTcp()
    - TcpThreadFunc() *- PascalCase*
    - tcpThreadFunc() *- camelCase*

## Class, structure, enum names and enum members
Class. structure and enum names must be written in PascalCase. No exceptions.

## Global functions, functions in namespace scope and methods
Any of these names must be written in PascalCase. Scope doesn't matter.

## Variables, constants, function parameters
Any of these names must be written in snake\_case. Scope doesn't matter.

# Doxygen
Use '**//! *text here***' for single line doxygen comment
Use '**/\*! *text here* \*/ **' for multi line doxygen comment
