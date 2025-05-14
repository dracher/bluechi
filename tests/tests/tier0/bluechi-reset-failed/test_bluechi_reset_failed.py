#
# Copyright Contributors to the Eclipse BlueChi project
#
# SPDX-License-Identifier: LGPL-2.1-or-later

import logging
from typing import Dict

from bluechi_test.config import BluechiAgentConfig, BluechiControllerConfig
from bluechi_test.machine import BluechiAgentMachine, BluechiControllerMachine
from bluechi_test.service import Option, Section, SimpleRemainingService
from bluechi_test.test import BluechiTest

LOGGER = logging.getLogger(__name__)
NODE_FOO = "node-foo"


def exec(ctrl: BluechiControllerMachine, nodes: Dict[str, BluechiAgentMachine]):

    node_foo = nodes[NODE_FOO]

    failed_service_node_foo = SimpleRemainingService(name="simple.service")
    failed_service_node_foo.set_option(Section.Service, Option.ExecStart, "/s")
    failed_service_node_ctrl = SimpleRemainingService(name="simple.service")
    failed_service_node_ctrl.set_option(Section.Service, Option.ExecStart, "/s")

    node_foo.install_systemd_service(failed_service_node_foo)
    ctrl.install_systemd_service(failed_service_node_ctrl)

    ctrl.bluechictl.start_unit(NODE_FOO, failed_service_node_ctrl.name)
    ctrl.systemctl_start_and_wait(failed_service_node_foo.name)

    assert node_foo.wait_for_unit_state_to_be(failed_service_node_ctrl.name, "failed")
    assert ctrl.wait_for_unit_state_to_be(failed_service_node_foo.name, "failed")

    ctrl.bluechictl.reset_failed()

    assert node_foo.wait_for_unit_state_to_be(failed_service_node_ctrl.name, "inactive")
    assert ctrl.wait_for_unit_state_to_be(failed_service_node_foo.name, "inactive")


def test_bluechi_reset_failed(
    bluechi_test: BluechiTest,
    bluechi_node_default_config: BluechiAgentConfig,
    bluechi_ctrl_default_config: BluechiControllerConfig,
):
    node_foo_cfg = bluechi_node_default_config.deep_copy()
    node_foo_cfg.node_name = NODE_FOO

    bluechi_test.add_bluechi_agent_config(node_foo_cfg)

    bluechi_ctrl_default_config.allowed_node_names = [NODE_FOO]
    bluechi_test.set_bluechi_controller_config(bluechi_ctrl_default_config)

    bluechi_test.run(exec)
