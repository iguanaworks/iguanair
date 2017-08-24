all: iguanaTest.exe

IguanaIR.dll: IguanaIR.cs
	mcs -target:library $^

iguanaTest.exe: iguanaTest.cs IguanaIR.dll
	mcs -reference:IguanaIR.dll iguanaTest.cs

clean:
	rm -f *~ IguanaIR.dll iguanaTest.exe

test: all
	mono iguanaTest.exe
