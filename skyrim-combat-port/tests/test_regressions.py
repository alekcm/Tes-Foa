"""Статические регрессии: не заменяют Windows-сборку и проверку в игре."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import check_rules


class SafetyRegressions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (ROOT / 'native/src/main.cpp').read_text(encoding='utf-8')
        cls.rules = (ROOT / 'native/rules/Rules.txt').read_text(encoding='utf-8')

    def test_world_lifecycle_and_deferred_tasks_are_guarded(self):
        entry = self.source.split('SKSE_PLUGIN_LOAD(', 1)[1]
        self.assertIn('RegisterListener(OnSKSEMessage)', entry)
        self.assertNotIn('LoadRules();', entry)
        self.assertIn('case SKSE::MessagingInterface::kDataLoaded:', self.source)
        self.assertIn('case SKSE::MessagingInterface::kPreLoadGame:', self.source)
        self.assertIn('generation != g_worldGeneration.load()', self.source)
        self.assertIn('g_worldReady.store(a_message->data != nullptr)', self.source)

    def test_bad_rule_disables_whole_rule(self):
        self.assertIn('current.valid = false;', self.source)
        self.assertIn('!a_rule.valid', self.source)
        self.assertIn('std::isfinite(a_rule.chip)', self.source)
        for value in ('nan', 'inf', '-1', '1oops'):
            self.assertTrue(check_rules.check_line('chip', value), value)

    def test_shield_bash_requires_shield_and_bash(self):
        section = self.rules.split('[shield-bash-staggers-unblocked]', 1)[1].split('[', 1)[0]
        self.assertIn('hit-flags = bash', section)
        self.assertIn('attacker.shield = yes', section)
        self.assertIn('player.blocking = no', section)

    def test_ranged_rules_do_not_require_bash_reach(self):
        for rule in ('arrow-and-bolt-through-weapon-block', 'spell-through-weapon-block'):
            section = self.rules.split('[' + rule + ']', 1)[1].split('[', 1)[0]
            self.assertIn('reach = no', section)

    def test_no_unsafe_immediate_effect_fallback(self):
        block = self.source.split('if (!tasks) {', 1)[1].split('const auto generation', 1)[0]
        self.assertNotIn('RunEffectNow', block)
        self.assertIn('return;', block)

    def test_actor_handle_kept_alive_for_pending_fires(self):
        self.assertIn('auto              source = it->source.get();', self.source)
        self.assertNotIn('it->source.get().get()', self.source)


if __name__ == '__main__':
    unittest.main()
