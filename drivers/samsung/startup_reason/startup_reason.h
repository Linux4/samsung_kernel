#ifndef START_UP_REASON_H
#define START_UP_REASON_H


/*

*/
struct startup_reason_source
{
	char * name ;
	struct device *dev ;
	struct device *parent;
	struct module *owner;
	char * (*get_reasons_name)(struct startup_reason_source * source ,int index);
};


int register_startup_reasons(struct startup_reason_source * source,int number_of_reasons,struct device * parent);
void unregister_startup_reasons(struct startup_reason_source * source);





#endif //START_UP_REASON_H
