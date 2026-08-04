.PHONY: all test clean

all: test

test:
	$(MAKE) -C assignments/01-data-lab test

clean:
	$(MAKE) -C assignments/01-data-lab clean
