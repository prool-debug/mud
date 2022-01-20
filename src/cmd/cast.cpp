#include "cast.h"

#include "entities/char.h"
#include "entities/obj.h"
#include "entities/room.h"
#include "structs/structs.h"
#include "comm.h"
#include "skills.h"
#include "magic/spells.h"
#include "magic/magic_utils.h"
#include "features.h"
#include "classes/class_spell_slots.h"
#include "magic/spells_info.h"
#include "handler.h"
#include "screen.h"

/*
 * do_cast is the entry point for PC-casted spells.  It parses the arguments,
 * determines the spell number and finds a target, throws the die to see if
 * the spell can be cast, checks for sufficient mana and subtracts it, and
 * passes control to CastSpell().
 */
void do_cast(CharacterData *ch, char *argument, int/* cmd*/, int /*subcmd*/) {
	CharacterData *tch;
	ObjectData *tobj;
	RoomData *troom;

	char *s, *t;
	int i, spellnum, spell_subst, target = 0;

	if (IS_NPC(ch) && AFF_FLAGGED(ch, EAffectFlag::AFF_CHARM))
		return;

	if (AFF_FLAGGED(ch, EAffectFlag::AFF_SILENCE) || AFF_FLAGGED(ch, EAffectFlag::AFF_STRANGLED)) {
		send_to_char("Вы не смогли вымолвить и слова.\r\n", ch);
		return;
	}
	if (ch->haveCooldown(SKILL_GLOBAL_COOLDOWN)) {
		send_to_char("Вам нужно набраться сил.\r\n", ch);
		return;
	};

	if (!ch->affected.empty()) {
		for (const auto &aff : ch->affected) {
			if (aff->location == APPLY_PLAQUE && number(1, 100) < 10) { // лихорадка 10% фэйл закла
				send_to_char("Вас трясет лихорадка, вы не смогли сконцентрироваться на произнесении заклинания.\r\n",
							 ch);
				return;
			}
			if (aff->location == APPLY_MADNESS && number(1, 100) < 20) { // безумие 20% фэйл закла
				send_to_char("Начав безумно кричать, вы забыли, что хотели произнести.\r\n", ch);
				return;
			}
		}
	}

	if (ch->is_morphed()) {
		send_to_char("Вы не можете произносить заклинания в звериной форме.\r\n", ch);
		return;
	}

	// get: blank, spell name, target name
	s = strtok(argument, "'*!");
	if (s == nullptr) {
		send_to_char("ЧТО вы хотите колдовать?\r\n", ch);
		return;
	}
	s = strtok(nullptr, "'*!");
	if (s == nullptr) {
		send_to_char("Название заклинания должно быть заключено в символы : ' или * или !\r\n", ch);
		return;
	}
	t = strtok(nullptr, "\0");

	spellnum = FixNameAndFindSpellNum(s);
	spell_subst = spellnum;

	// Unknown spell
	if (spellnum < 1 || spellnum > SPELLS_COUNT) {
		send_to_char("И откуда вы набрались таких выражений?\r\n", ch);
		return;
	}

	// Caster is lower than spell level
	if ((!IS_SET(GET_SPELL_TYPE(ch, spellnum), SPELL_TEMP | SPELL_KNOW) ||
		GET_REAL_REMORT(ch) < MIN_CAST_REM(spell_info[spellnum], ch)) &&
		(GET_REAL_LEVEL(ch) < kLevelGreatGod) && (!IS_NPC(ch))) {
		if (GET_REAL_LEVEL(ch) < MIN_CAST_LEV(spell_info[spellnum], ch)
			|| GET_REAL_REMORT(ch) < MIN_CAST_REM(spell_info[spellnum], ch)
			|| PlayerClass::slot_for_char(ch, spell_info[spellnum].slot_forc[(int) GET_CLASS(ch)][(int) GET_KIN(ch)])
				<= 0) {
			send_to_char("Рано еще вам бросаться такими словами!\r\n", ch);
			return;
		} else {
			send_to_char("Было бы неплохо изучить, для начала, это заклинание...\r\n", ch);
			return;
		}
	}

	// Caster havn't slot
	if (!GET_SPELL_MEM(ch, spellnum) && !IS_IMMORTAL(ch)) {
		if (can_use_feat(ch, SPELL_SUBSTITUTE_FEAT)
			&& (spellnum == SPELL_CURE_LIGHT || spellnum == SPELL_CURE_SERIOUS
				|| spellnum == SPELL_CURE_CRITIC || spellnum == SPELL_HEAL)) {
			for (i = 1; i <= SPELLS_COUNT; i++) {
				if (GET_SPELL_MEM(ch, i) &&
					spell_info[i].slot_forc[(int) GET_CLASS(ch)][(int) GET_KIN(ch)] ==
						spell_info[spellnum].slot_forc[(int) GET_CLASS(ch)][(int) GET_KIN(ch)]) {
					spell_subst = i;
					break;
				}
			}
			if (i > SPELLS_COUNT) {
				send_to_char("У вас нет заученных заклинаний этого круга.\r\n", ch);
				return;
			}
		} else {
			send_to_char("Вы совершенно не помните, как произносится это заклинание...\r\n", ch);
			return;
		}
	}

	// Find the target
	if (t != nullptr)
		one_argument(t, arg);
	else
		*arg = '\0';

	target = FindCastTarget(spellnum, arg, ch, &tch, &tobj, &troom);

	if (target && (tch == ch) && spell_info[spellnum].violent) {
		send_to_char("Лекари не рекомендуют использовать ЭТО на себя!\r\n", ch);
		return;
	}
	if (!target) {
		send_to_char("Тяжеловато найти цель вашего заклинания!\r\n", ch);
		return;
	}
	if (!IS_SET(GET_SPELL_TYPE(ch, spellnum), SPELL_TEMP) && AFF_FLAGGED(ch, EAffectFlag::AFF_DOMINATION)) {
		send_to_char("На данной арене вы можете колдовать только временные заклинания!\r\n", ch);
		return;
	}

	// You throws the dice and you takes your chances.. 101% is total failure
	// Чтобы в бой не вступал с уже взведенной заклинашкой !!!
	ch->set_cast(0, 0, 0, 0, 0);
	if (!CalculateCastSuccess(ch, tch, SAVING_STABILITY, spellnum)) {
		if (!(IS_IMMORTAL(ch) || GET_GOD_FLAG(ch, GF_GODSLIKE)))
			WAIT_STATE(ch, kPulseViolence);
		if (GET_SPELL_MEM(ch, spell_subst)) {
			GET_SPELL_MEM(ch, spell_subst)--;
		}
		if (!IS_NPC(ch) && !IS_IMMORTAL(ch) && PRF_FLAGGED(ch, PRF_AUTOMEM))
			MemQ_remember(ch, spell_subst);
		//log("[DO_CAST->AFFECT_TOTAL] Start");
		affect_total(ch);
		//log("[DO_CAST->AFFECT_TOTAL] Stop");
		if (!tch || !SendSkillMessages(0, ch, tch, spellnum))
			send_to_char("Вы не смогли сосредоточиться!\r\n", ch);
	} else        // cast spell returns 1 on success; subtract mana & set waitstate
	{
		if (ch->get_fighting() && !IS_IMPL(ch)) {
			ch->set_cast(spellnum, spell_subst, tch, tobj, troom);
			sprintf(buf,
					"Вы приготовились применить заклинание %s'%s'%s%s.\r\n",
					CCCYN(ch, C_NRM), spell_info[spellnum].name, CCNRM(ch, C_NRM),
					tch == ch ? " на себя" : tch ? " на $N3" : tobj ? " на $o3" : troom ? " на всех" : "");
			act(buf, false, ch, tobj, tch, TO_CHAR);
		} else if (CastSpell(ch, tch, tobj, troom, spellnum, spell_subst) >= 0) {
			if (!(WAITLESS(ch) || CHECK_WAIT(ch)))
				WAIT_STATE(ch, kPulseViolence);
		}
	}
}


