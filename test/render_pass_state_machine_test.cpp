#include "listeners/render_pass_listener.hpp"
#include "render_pass.hpp"
#include "render_pass_state_machine.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

namespace
{
/** Records every call as a word, and claims tonemap draws while claims_tonemap is set. */
class RecordingListener : public ds3ea::IRenderPassListener
{
public:
	void on_gbuffer_begin(reshade::api::command_list* /*cmd_list*/) override
	{
		calls.emplace_back("gbuffer_begin");
	}

	void on_gbuffer_end(reshade::api::command_list* /*cmd_list*/) override
	{
		calls.emplace_back("gbuffer_end");
	}

	void on_tonemap_begin(reshade::api::command_list* /*cmd_list*/) override
	{
		calls.emplace_back("tonemap_begin");
	}

	ds3ea::DrawAction on_tonemap_draw(reshade::api::command_list* /*cmd_list*/, const ds3ea::DrawCallArgs& /*draw*/,
		bool claimed) override
	{
		calls.emplace_back(claimed ? "tonemap_draw_claimed" : "tonemap_draw");
		return claims_tonemap ? ds3ea::DrawAction::skip : ds3ea::DrawAction::keep;
	}

	void on_tonemap_end(reshade::api::command_list* /*cmd_list*/) override
	{
		calls.emplace_back("tonemap_end");
	}

	void on_game_present() override
	{
		calls.emplace_back("present");
	}

	std::vector<std::string> calls;
	bool claims_tonemap = false;
};

TEST(RenderPassStateMachine, AGBufferRunBeginsOnceAndEndsAtTheNextPass)
{
	RecordingListener listener;
	const std::array<ds3ea::IRenderPassListener*, 1> listeners = { &listener };
	ds3ea::RenderPassStateMachine state_machine;

	for (const ds3ea::RenderPass pass : { ds3ea::RenderPass::gbuffer, ds3ea::RenderPass::unknown,
			 ds3ea::RenderPass::gbuffer, ds3ea::RenderPass::decal })
		EXPECT_EQ(state_machine.on_draw(pass, nullptr, {}, listeners), ds3ea::DrawAction::keep);

	EXPECT_EQ(listener.calls, (std::vector<std::string>{ "gbuffer_begin", "gbuffer_end" }));
	EXPECT_EQ(state_machine.get_open_pass(), ds3ea::RenderPass::decal);
}

TEST(RenderPassStateMachine, EndClosesTheOpenPass)
{
	RecordingListener listener;
	const std::array<ds3ea::IRenderPassListener*, 1> listeners = { &listener };
	ds3ea::RenderPassStateMachine state_machine;

	EXPECT_EQ(state_machine.on_draw(ds3ea::RenderPass::tonemap, nullptr, {}, listeners), ds3ea::DrawAction::keep);
	state_machine.end(nullptr, listeners);
	state_machine.end(nullptr, listeners);

	EXPECT_EQ(listener.calls, (std::vector<std::string>{ "tonemap_begin", "tonemap_draw", "tonemap_end" }));
	EXPECT_EQ(state_machine.get_open_pass(), ds3ea::RenderPass::unknown);
}

TEST(RenderPassStateMachine, EachTonemapDrawIsAskedSeparately)
{
	RecordingListener listener;
	const std::array<ds3ea::IRenderPassListener*, 1> listeners = { &listener };
	ds3ea::RenderPassStateMachine state_machine;

	listener.claims_tonemap = true;
	EXPECT_EQ(state_machine.on_draw(ds3ea::RenderPass::tonemap, nullptr, {}, listeners), ds3ea::DrawAction::skip);
	listener.claims_tonemap = false;
	EXPECT_EQ(state_machine.on_draw(ds3ea::RenderPass::tonemap, nullptr, {}, listeners), ds3ea::DrawAction::keep);

	EXPECT_EQ(listener.calls, (std::vector<std::string>{ "tonemap_begin", "tonemap_draw", "tonemap_draw" }));
}

TEST(RenderPassStateMachine, LaterListenersSeeTheDrawIsClaimed)
{
	RecordingListener claimer;
	RecordingListener observer;
	claimer.claims_tonemap = true;
	const std::array<ds3ea::IRenderPassListener*, 2> listeners = { &claimer, &observer };
	ds3ea::RenderPassStateMachine state_machine;

	EXPECT_EQ(state_machine.on_draw(ds3ea::RenderPass::tonemap, nullptr, {}, listeners), ds3ea::DrawAction::skip);
	EXPECT_EQ(state_machine.on_draw(ds3ea::RenderPass::color_grading, nullptr, {}, listeners), ds3ea::DrawAction::keep);

	EXPECT_EQ(claimer.calls, (std::vector<std::string>{ "tonemap_begin", "tonemap_draw", "tonemap_end" }));
	EXPECT_EQ(observer.calls,
		(std::vector<std::string>{ "tonemap_begin", "tonemap_draw_claimed", "tonemap_end" }));
}

TEST(RenderPassStateMachine, AttachOrderIsThePriority)
{
	RecordingListener observer;
	RecordingListener claimer;
	RecordingListener late_claimer;
	claimer.claims_tonemap = true;
	late_claimer.claims_tonemap = true;
	const std::array<ds3ea::IRenderPassListener*, 3> listeners = { &observer, &claimer, &late_claimer };
	ds3ea::RenderPassStateMachine state_machine;

	EXPECT_EQ(state_machine.on_draw(ds3ea::RenderPass::tonemap, nullptr, {}, listeners), ds3ea::DrawAction::skip);
	EXPECT_EQ(observer.calls, (std::vector<std::string>{ "tonemap_begin", "tonemap_draw" }));
	EXPECT_EQ(late_claimer.calls, (std::vector<std::string>{ "tonemap_begin", "tonemap_draw_claimed" }));
}
} // namespace
