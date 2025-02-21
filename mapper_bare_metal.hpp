#pragma once

#include "mapper_base.hpp"

#include <xbt/log.h>

class Mapper_Bare_Metal : public Mapper_Base
{
  public:
    Mapper_Bare_Metal(common_t *common, scheduler_t &scheduler);
    ~Mapper_Bare_Metal();

    void start() override;
};

typedef Mapper_Bare_Metal mapper_bare_metal_t;

void *mapper_bare_metal_thread_function(void *arg);
