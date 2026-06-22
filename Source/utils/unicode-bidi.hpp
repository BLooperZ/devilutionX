#pragma once

#include <cstddef>
#include <iterator>
#include <span>
#include <string_view>

#include <SheenBidi/SheenBidi.h>

#include "utils/sheen_bidi.hpp"

namespace devilution {

class BidiVisualText {
public:
	struct VisualChar {
		char32_t codepoint;
		size_t logicalByte;
		size_t cpLen;
		bool isRTL;
	};

	explicit BidiVisualText(std::string_view text)
	    : text_(text)
	{
		algorithm_ = sb::Algorithm::create(text_);
		paragraph_ = algorithm_.createParagraph(0, static_cast<SBUInteger>(text_.size()), SBLevelDefaultLTR);
		line_ = paragraph_.createLine(0, static_cast<SBUInteger>(text_.size()));
		runs_ = line_.runs();
		baseLevel_ = SBParagraphGetBaseLevel(paragraph_.get());
	}

	class iterator {
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = VisualChar;
		using difference_type = std::ptrdiff_t;
		using pointer = const VisualChar *;
		using reference = const VisualChar &;

		const VisualChar &operator*() const { return current_; }
		const VisualChar *operator->() const { return &current_; }

		iterator &operator++()
		{
			advance();
			return *this;
		}

		bool operator==(const iterator &other) const { return done_ == other.done_; }
		bool operator!=(const iterator &other) const { return !(*this == other); }

	private:
		friend class BidiVisualText;

		const BidiVisualText *owner_;
		size_t runIndex_;
		SBUInteger offset_;
		VisualChar current_ {};
		bool done_;

		iterator(const BidiVisualText *owner, bool end)
		    : owner_(owner)
		    , runIndex_(0)
		    , offset_(0)
		    , done_(end)
		{
			if (!done_) {
				if (owner_->runs_.empty()) {
					done_ = true;
					return;
				}
				const auto &firstRun = owner_->runs_[0];
				if (sb::IsRTL(firstRun))
					offset_ = firstRun.offset + firstRun.length;
				else
					offset_ = firstRun.offset;
				advance();
			}
		}

		void advance()
		{
			for (;;) {
				if (runIndex_ >= owner_->runs_.size()) {
					done_ = true;
					return;
				}

				const auto &run = owner_->runs_[runIndex_];
				const size_t runStart = run.offset;
				const size_t runLength = run.length;
				const bool isRTL = sb::IsRTL(run);

				if (runLength == 0) {
					++runIndex_;
					continue;
				}

				const std::string_view runText = owner_->text_.substr(runStart, runLength);

				if (isRTL) {
					if (offset_ <= runStart) {
						advanceRun();
						continue;
					}
					const SBUInteger oldRelative = offset_ - runStart;
					SBUInteger index = oldRelative;
					const SBCodepoint cp = sb::CodepointDecodePrevious(runText, index);
					if (cp == SBCodepointInvalid) {
						done_ = true;
						return;
					}
					const size_t cpLen = oldRelative - index;
					current_ = { static_cast<char32_t>(cp), runStart + index, cpLen, true };
					offset_ = runStart + index;
					return;
				}

				const size_t runEnd = runStart + runLength;
				if (offset_ >= runEnd) {
					advanceRun();
					continue;
				}
				const SBUInteger oldRelative = offset_ - runStart;
				SBUInteger index = oldRelative;
				const SBCodepoint cp = sb::CodepointDecodeNext(runText, index);
				if (cp == SBCodepointInvalid) {
					done_ = true;
					return;
				}
				const size_t cpLen = index - oldRelative;
				current_ = { static_cast<char32_t>(cp), runStart + oldRelative, cpLen, false };
				offset_ = runStart + index;
				return;
			}
		}

		void advanceRun()
		{
			++runIndex_;
			if (runIndex_ < owner_->runs_.size()) {
				const auto &nextRun = owner_->runs_[runIndex_];
				if (sb::IsRTL(nextRun))
					offset_ = nextRun.offset + nextRun.length;
				else
					offset_ = nextRun.offset;
			}
		}
	};

	[[nodiscard]] iterator begin() const { return iterator(this, false); }
	[[nodiscard]] iterator end() const { return iterator(this, true); }

private:
	std::string_view text_;
	sb::Algorithm algorithm_;
	sb::Paragraph paragraph_;
	sb::Line line_;
	std::span<const SBRun> runs_;
	SBLevel baseLevel_;
};

} // namespace devilution
