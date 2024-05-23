typedef char *va_list;

//
// By the i386 calling convention, when a C function with 
// variadic arguments, f(last, ..., first), is called, 
// the stack looks as shown below. 'Last' is logically
// the first argument, while 'first' is logically the last.
// 'First' and 'last' are not in logical order but reflect their order on
// the stack. We call the argument pushed first at the highest address 'first'
// and the one pushed last at the lowest address 'last':
//
// [    frst    ]
// [    argN    ]
// [    ....    ]
// [    arg1    ]
// [    arg0    ] <- ap + sizeof(last)
// [    last    ] <- ap
// [ ReturnAddr ]
//

// gcc promotes any arguments smaller than an int to an int
// So we must comply to that when retrieving the arguments from the stack
#define	__va_promote(x) \
	(((sizeof(x) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

// Initialize 'ap' with 'last,' where 'last' is the first argument pushed onto the stack
// and thus is at the lowest address. Take the arguably most commonly used example,
// printf(char *s, ...), where 'last' would be 's'. 'ap' is of type 'va_list',
// aka. char*.
#define va_start(ap, last) \
    (ap = (char *)&last + __va_promote(last))

// Grab one argument from the argument list at 'ap' -
// move 'ap' up in the argument list by sizeof(type) and "return"
// the value dereferenced from where 'ap' was previously.
#define va_arg(ap, type) \
    (*(type *)((ap += sizeof(type)) - sizeof(type)))

// Do nothing for i386. Only included for completeness.
#define va_end(ap)
