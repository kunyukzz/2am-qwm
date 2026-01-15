#include "qwm.h"

int main(void)
{
    qwm_t *qwm = qwm_init();
    if (!qwm) return 1;

    qwm_run(qwm);

    return 0;
}
