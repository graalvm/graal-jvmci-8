// Simple JavaScript tests for the gate
function assertEQ(expect, actual) {
    if (expect != actual) {
        throw "error: " + expect + " != " + actual;
    }
}

function fib(n) {
    return (n <= 1) ? 1 : fib(n-1)+fib(n-1);
}

assertEQ(fib(25), 16777216);
