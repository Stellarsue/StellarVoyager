import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[1] / "automation/news_publisher.py"
spec = importlib.util.spec_from_file_location("news_publisher", SCRIPT)
news = importlib.util.module_from_spec(spec)
spec.loader.exec_module(news)


class NewsPublisherTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.paths = patch.multiple(news, CONFIG_DIR=root / "config", DATA_DIR=root / "data",
                                    STATE_DIR=root / "state", CONFIG_PATH=root / "config/news.json",
                                    STATE_PATH=root / "state/news-state.json")
        self.paths.start()
        self.addCleanup(self.paths.stop)
        news.write_json(news.CONFIG_PATH, {"enabled": True, "feeds": ["https://example.com/feed"]})

    def article(self):
        return {"title": "A new chip", "url": "https://example.com/chip",
                "description": "Chip details", "source": "https://example.com/feed"}

    def test_success_records_url_and_prevents_reposting(self):
        article = self.article()
        with patch.object(news, "find_news", side_effect=[article, None]) as find, \
             patch.object(news, "chatgpt_draft", return_value="一条科技新闻\n来源：https://example.com/chip"), \
             patch.object(news, "weibo_publish") as publish:
            self.assertEqual(news.run(), 0)
            self.assertEqual(news.run(), 0)
        self.assertEqual(publish.call_count, 1)
        self.assertIn(article["url"], find.call_args_list[1].args[1])
        self.assertEqual(news.state()["last_status"], "skipped")

    def test_uncertain_publication_is_not_retried(self):
        article = self.article()
        with patch.object(news, "find_news", return_value=article), \
             patch.object(news, "chatgpt_draft", return_value="一条科技新闻"), \
             patch.object(news, "weibo_publish", side_effect=news.PublicationUncertain("请核查")):
            self.assertEqual(news.run(), 2)
        self.assertIn(article["url"], news.state()["uncertain"])

    def test_rss_and_atom_dates_are_parsed(self):
        rss = b"<rss><channel><item><title>News</title><link>https://a.test/1</link>" \
              b"<description>New &lt;b&gt;chip&lt;/b&gt;</description>" \
              b"<pubDate>Thu, 29 Oct 2026 00:00:00 GMT</pubDate></item></channel></rss>"
        atom = b'<feed xmlns="http://www.w3.org/2005/Atom"><entry><title>Story</title>' \
               b'<link rel="alternate" href="https://a.test/2"/><summary>AI</summary>' \
               b'<updated>2026-10-29T00:00:00Z</updated></entry></feed>'
        self.assertEqual(news.parse_feed(rss, "rss")[0]["description"], "New chip")
        self.assertEqual(news.parse_feed(atom, "atom")[0]["url"], "https://a.test/2")


if __name__ == "__main__":
    unittest.main()
