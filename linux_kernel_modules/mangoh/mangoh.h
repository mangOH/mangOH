#ifndef MANGOH_H
#define MANGOH_H

int mangoh_find_contiguous_irqs(unsigned int num_required, unsigned long irq_flags);
struct mangoh_descriptor {
	char* type; // TODO: why is this needed?
	int (*map)(struct platform_device* pdev);
	int (*unmap)(struct platform_device* pdev);
};

#endif // MANGOH_H
