import Std

namespace Companion

inductive Mode where | normal | advantage | disadvantage
  deriving DecidableEq, Repr

def mode (adv dis : Bool) : Mode :=
  if adv == dis then .normal else if adv then .advantage else .disadvantage

def keep : Mode → Int → Int → Int
  | .normal, a, _ => a
  | .advantage, a, b => max a b
  | .disadvantage, a, b => min a b

def abilityMod (score : Int) := score / 2 - 5
def profBonus (level : Nat) := 2 + (level-1)/4

theorem opposite_cancel : mode true true = .normal := rfl

theorem advantage_keeps_high (a b : Int) :
    a ≤ keep .advantage a b ∧ b ≤ keep .advantage a b := by
  simp only [keep, Int.max_def]; split <;> omega

theorem disadvantage_keeps_low (a b : Int) :
    keep .disadvantage a b ≤ a ∧ keep .disadvantage a b ≤ b := by
  simp only [keep, Int.min_def]; split <;> omega

theorem kept_is_rolled (m : Mode) (a b : Int) :
    keep m a b = a ∨ keep m a b = b := by
  by_cases h : a ≤ b <;> cases m <;> simp [keep, Int.max_def, Int.min_def, h]

theorem kept_bounds (m : Mode) (a b : Int)
    (ha : 1 ≤ a ∧ a ≤ 20) (hb : 1 ≤ b ∧ b ≤ 20) :
    1 ≤ keep m a b ∧ keep m a b ≤ 20 := by
  rcases kept_is_rolled m a b with h | h <;> rw [h] <;> assumption

theorem ability_is_floor (s : Int) :
    2 * abilityMod s ≤ s - 10 ∧ s - 10 < 2 * (abilityMod s + 1) := by
  unfold abilityMod
  omega

theorem proficiency_bounds (n : Nat) (lo : 1 ≤ n) (hi : n ≤ 20) :
    2 ≤ profBonus n ∧ profBonus n ≤ 6 := by
  unfold profBonus
  omega

theorem total_bounds (m : Mode) (a b k : Int) (ha : 1 ≤ a ∧ a ≤ 20) (hb : 1 ≤ b ∧ b ≤ 20) :
    1 + k ≤ keep m a b + k ∧ keep m a b + k ≤ 20 + k := by
  have h := kept_bounds m a b ha hb
  omega

-- Each die can have a different number of sides.
inductive ValidPool : List Nat → List Nat → Prop
  | nil : ValidPool [] []
  | cons {f s fs ss} : 1 ≤ f → f ≤ s → ValidPool fs ss → ValidPool (f::fs) (s::ss)

theorem pool_bounds {faces sides : List Nat} (h : ValidPool faces sides) :
    faces.length ≤ faces.sum ∧ faces.sum ≤ sides.sum := by
  induction h with
  | nil => simp
  | cons lo hi tail ih => simp only [List.length_cons, List.sum_cons]; omega

-- Resource math uses spent points, matching session_rules.h.
def pointsAfterConversion (poolMax spent slotLevel : Nat) := poolMax - (spent - slotLevel)

theorem conversion_stays_in_pool (poolMax spent slotLevel : Nat) :
    pointsAfterConversion poolMax spent slotLevel ≤ poolMax := by
  unfold pointsAfterConversion
  omega

theorem conversion_gain (poolMax spent slotLevel : Nat) (valid : spent ≤ poolMax) :
    pointsAfterConversion poolMax spent slotLevel = (poolMax - spent) + min spent slotLevel := by
  unfold pointsAfterConversion
  omega

theorem affordable_point_spend (available cost : Nat) (valid : cost ≤ available) :
    available - cost + cost = available := by omega

end Companion

-- The Mac verifier compares this specification output with the exact firmware header.
def main : IO Unit := do
  for adv in [false,true] do
    for dis in [false,true] do
      for a in [1:21] do
        for b in [1:21] do
          IO.println (Companion.keep (Companion.mode adv dis) a b)
  for s in [1:31] do IO.println (Companion.abilityMod s)
  for level in [1:21] do IO.println (Companion.profBonus level)
